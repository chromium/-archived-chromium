// Copyright 2008, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "webkit/tools/test_shell/temp/navigation_controller_base.h"

#include <algorithm>

#include "webkit/tools/test_shell/temp/navigation_entry.h"
#include "base/logging.h"

NavigationControllerBase::NavigationControllerBase()
    : pending_entry_(NULL),
      last_committed_entry_index_(-1),
      pending_entry_index_(-1) {
}

NavigationControllerBase::~NavigationControllerBase() {
  // NOTE: This does NOT invoke Reset as Reset is virtual.
  ResetInternal();
}

void NavigationControllerBase::Reset() {
  ResetInternal();

  last_committed_entry_index_ = -1;
}

NavigationEntry* NavigationControllerBase::GetActiveEntry() const {
  NavigationEntry* entry = pending_entry_;
  if (!entry)
    entry = GetLastCommittedEntry();
  return entry;
}

int NavigationControllerBase::GetCurrentEntryIndex() const {
  if (pending_entry_index_ != -1)
    return pending_entry_index_;
  return last_committed_entry_index_;
}

NavigationEntry* NavigationControllerBase::GetLastCommittedEntry() const {
  if (last_committed_entry_index_ == -1)
    return NULL;
  return entries_[last_committed_entry_index_];
}

int NavigationControllerBase::GetEntryIndexWithPageID(
    TabContentsType type, int32 page_id) const {
  for (int i = static_cast<int>(entries_.size())-1; i >= 0; --i) {
    if (entries_[i]->GetType() == type && entries_[i]->GetPageID() == page_id)
      return i;
  }
  return -1;
}

NavigationEntry* NavigationControllerBase::GetEntryWithPageID(
    TabContentsType type, int32 page_id) const {
  int index = GetEntryIndexWithPageID(type, page_id);
  return (index != -1) ? entries_[index] : NULL;
}

NavigationEntry* NavigationControllerBase::GetEntryAtOffset(int offset) const {
  int index = last_committed_entry_index_ + offset;
  if (index < 0 || index >= GetEntryCount())
    return NULL;

  return entries_[index];
}

bool NavigationControllerBase::CanStop() const {
  // TODO(darin): do we have something pending that we can stop?
  return false;
}

bool NavigationControllerBase::CanGoBack() const {
  return entries_.size() > 1 && GetCurrentEntryIndex() > 0;
}

bool NavigationControllerBase::CanGoForward() const {
  int index = GetCurrentEntryIndex();
  return index >= 0 && index < (static_cast<int>(entries_.size()) - 1);
}

void NavigationControllerBase::GoBack() {
  DCHECK(CanGoBack());

  // Base the navigation on where we are now...
  int current_index = GetCurrentEntryIndex();

  DiscardPendingEntry();

  pending_entry_index_ = current_index - 1;
  NavigateToPendingEntry(false);
}

void NavigationControllerBase::GoForward() {
  DCHECK(CanGoForward());

  // Base the navigation on where we are now...
  int current_index = GetCurrentEntryIndex();

  DiscardPendingEntry();

  pending_entry_index_ = current_index + 1;
  NavigateToPendingEntry(false);
}

void NavigationControllerBase::GoToIndex(int index) {
  DCHECK(index >= 0);
  DCHECK(index < static_cast<int>(entries_.size()));

  DiscardPendingEntry();

  pending_entry_index_ = index;
  NavigateToPendingEntry(false);
}

void NavigationControllerBase::GoToOffset(int offset) {
  int index = last_committed_entry_index_ + offset;
  if (index < 0 || index >= GetEntryCount())
    return;

  GoToIndex(index);
}

void NavigationControllerBase::Stop() {
  DCHECK(CanStop());

  // TODO(darin): we probably want to just call Stop on the active tab
  // contents, but should we also call DiscardPendingEntry?
  NOTREACHED() << "implement me";
}

void NavigationControllerBase::Reload() {
  // Base the navigation on where we are now...
  int current_index = GetCurrentEntryIndex();

  // If we are no where, then we can't reload.  TODO(darin): We should add a
  // CanReload method.
  if (current_index == -1)
    return;

  DiscardPendingEntryInternal();

  pending_entry_index_ = current_index;
  entries_[pending_entry_index_]->SetTransition(PageTransition::RELOAD);
  NavigateToPendingEntry(true);
}

void NavigationControllerBase::LoadEntry(NavigationEntry* entry) {
  // When navigating to a new page, we don't know for sure if we will actually
  // end up leaving the current page.  The new page load could for example
  // result in a download or a 'no content' response (e.g., a mailto: URL).

  DiscardPendingEntryInternal();
  pending_entry_ = entry;
  NavigateToPendingEntry(false);
}

void NavigationControllerBase::DidNavigateToEntry(NavigationEntry* entry) {
  // If the entry is that of a page with PageID larger than any this Tab has
  // seen before, then consider it a new navigation.
  if (entry->GetPageID() > GetMaxPageID()) {
    InsertEntry(entry);
    return;
  }

  // Otherwise, we just need to update an existing entry with matching PageID.
  // If the existing entry corresponds to the entry which is pending, then we
  // must update the current entry index accordingly.  When navigating to the
  // same URL, a new PageID is not created.

  int existing_entry_index = GetEntryIndexWithPageID(entry->GetType(),
                                                     entry->GetPageID());
  NavigationEntry* existing_entry =
      (existing_entry_index != -1) ? entries_[existing_entry_index] : NULL;
  if (!existing_entry) {
    // No existing entry, then simply ignore this navigation!
    DLOG(WARNING) << "ignoring navigation for page: " << entry->GetPageID();
  } else if (existing_entry == pending_entry_) {
    // The given entry might provide a new URL... e.g., navigating back to a
    // page in session history could have resulted in a new client redirect.
    existing_entry->SetURL(entry->GetURL());
    existing_entry->SetContentState(entry->GetContentState());
    last_committed_entry_index_ = pending_entry_index_;
    pending_entry_index_ = -1;
    pending_entry_ = NULL;
    IndexOfActiveEntryChanged();
  } else if (pending_entry_ && pending_entry_->GetPageID() == -1 &&
             pending_entry_->GetURL() == existing_entry->GetURL()) {
    // Not a new navigation
    DiscardPendingEntry();
  } else {
    // The given entry might provide a new URL... e.g., navigating to a page
    // might result in a client redirect, which should override the URL of the
    // existing entry.
    existing_entry->SetURL(entry->GetURL());
    existing_entry->SetContentState(entry->GetContentState());

    // The navigation could have been issued by the renderer, so be sure that
    // we update our current index.
    last_committed_entry_index_ = existing_entry_index;
    IndexOfActiveEntryChanged();
  }

  delete entry;

  NotifyNavigationStateChanged();
}

void NavigationControllerBase::DiscardPendingEntry() {
  DiscardPendingEntryInternal();

  // Derived classes may do additional things in this case.
}

int NavigationControllerBase::GetIndexOfEntry(
    const NavigationEntry* entry) const {
  NavigationEntryList::const_iterator i = find(entries_.begin(), entries_.end(), entry);
  if (i == entries_.end())
    return -1;
  return static_cast<int>(i - entries_.begin());
}

void NavigationControllerBase::DiscardPendingEntryInternal() {
  if (pending_entry_index_ == -1)
    delete pending_entry_;
  pending_entry_ = NULL;
  pending_entry_index_ = -1;
}

void NavigationControllerBase::InsertEntry(NavigationEntry* entry) {
  DCHECK(entry->GetTransition() != PageTransition::AUTO_SUBFRAME);

  DiscardPendingEntryInternal();

  int current_size = static_cast<int>(entries_.size());

  // Prune any entry which are in front of the current entry
  if (current_size > 0) {
    while (last_committed_entry_index_ < (current_size - 1)) {
      PruneEntryAtIndex(current_size - 1);
      delete entries_[current_size - 1];
      entries_.pop_back();
      current_size--;
    }
    NotifyPrunedEntries();
  }

  entries_.push_back(entry);
  last_committed_entry_index_ = static_cast<int>(entries_.size()) - 1;

  NotifyNavigationStateChanged();
}

void NavigationControllerBase::ResetInternal() {
  // WARNING: this is invoked from the destructor, be sure not to invoke any
  // virtual methods from this.
  for (int i = 0, c = static_cast<int>(entries_.size()); i < c; ++i)
    delete entries_[i];
  entries_.clear();

  DiscardPendingEntryInternal();
}

#ifndef NDEBUG

void NavigationControllerBase::Dump() {
  int i,c;
  for (i = 1, c = static_cast<int>(entries_.size()); i < c; ++i) {
    DLOG(INFO) << entries_[i]->GetURL().spec();
  }
}

#endif
