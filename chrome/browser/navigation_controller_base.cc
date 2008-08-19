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

#include "chrome/browser/navigation_controller_base.h"

#include <algorithm>

#include "base/logging.h"
#include "chrome/browser/navigation_entry.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/notification_types.h"
#include "webkit/glue/webkit_glue.h"

// The maximum number of entries that a navigation controller can store.
const static size_t kMaxEntryCount = 50;

NavigationControllerBase::NavigationControllerBase()
    : pending_entry_(NULL),
      last_committed_entry_index_(-1),
      pending_entry_index_(-1),
      max_entry_count_(kMaxEntryCount) {
}

NavigationControllerBase::~NavigationControllerBase() {
  // NOTE: This does NOT invoke Reset as Reset is virtual.
  //ResetInternal();
}

/*void NavigationControllerBase::Reset() {
  ResetInternal();

  last_committed_entry_index_ = -1;
}*/

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
    TabContentsType type, SiteInstance* instance, int32 page_id) const {
  // The instance should only be specified for contents displaying web pages.
  // TODO(evanm): checking against NEW_TAB_UI and HTML_DLG here is lame.
  // It'd be nice for DomUIHost to just use SiteInstances for keeping content
  // separated properly.
  if (type != TAB_CONTENTS_WEB &&
      type != TAB_CONTENTS_NEW_TAB_UI &&
      type != TAB_CONTENTS_ABOUT_UI &&
      type != TAB_CONTENTS_HTML_DIALOG &&
      type != TAB_CONTENTS_VIEW_SOURCE &&
      type != TAB_CONTENTS_DEBUGGER)
    DCHECK(instance == NULL);

  for (int i = static_cast<int>(entries_.size()) - 1; i >= 0; --i) {
    if ((entries_[i]->GetType() == type) &&
        (entries_[i]->site_instance() == instance) &&
        (entries_[i]->GetPageID() == page_id))
      return i;
  }
  return -1;
}

NavigationEntry* NavigationControllerBase::GetEntryWithPageID(
    TabContentsType type, SiteInstance* instance, int32 page_id) const {
  int index = GetEntryIndexWithPageID(type, instance, page_id);
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
  if (!CanGoBack()) {
    NOTREACHED();
    return;
  }

  // Base the navigation on where we are now...
  int current_index = GetCurrentEntryIndex();

  DiscardPendingEntry();

  pending_entry_index_ = current_index - 1;
  NavigateToPendingEntry(false);
}

void NavigationControllerBase::GoForward() {
  if (!CanGoForward()) {
    NOTREACHED();
    return;
  }

  // Base the navigation on where we are now...
  int current_index = GetCurrentEntryIndex();

  DiscardPendingEntry();

  pending_entry_index_ = current_index + 1;
  NavigateToPendingEntry(false);
}

void NavigationControllerBase::GoToIndex(int index) {
  if (index < 0 || index >= static_cast<int>(entries_.size())) {
    NOTREACHED();
    return;
  }

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

  // TODO(pkasting): http://b/1113085 Should this use DiscardPendingEntry()?
  DiscardPendingEntryInternal();

  pending_entry_index_ = current_index;
  entries_[pending_entry_index_]->SetTransitionType(PageTransition::RELOAD);
  NavigateToPendingEntry(true);
}

void NavigationControllerBase::LoadEntry(NavigationEntry* entry) {
  // When navigating to a new page, we don't know for sure if we will actually
  // end up leaving the current page.  The new page load could for example
  // result in a download or a 'no content' response (e.g., a mailto: URL).

  // TODO(pkasting): http://b/1113085 Should this use DiscardPendingEntry()?
  DiscardPendingEntryInternal();
  pending_entry_ = entry;
  // TODO(brettw) the reinterpret cast can be removed once we combine the
  // NavigationController and the NavigationControllerBase.
  NotificationService::current()->Notify(
      NOTIFY_NAV_ENTRY_PENDING,
      Source<NavigationController>(reinterpret_cast<NavigationController*>(this)),
      NotificationService::NoDetails());
  NavigateToPendingEntry(false);
}

/* static */
void NavigationControllerBase::SetContentStateIfEmpty(
    NavigationEntry* entry) {
  if (entry->GetContentState().empty() &&
      (entry->GetType() == TAB_CONTENTS_WEB ||
       entry->GetType() == TAB_CONTENTS_NEW_TAB_UI ||
       entry->GetType() == TAB_CONTENTS_ABOUT_UI ||
       entry->GetType() == TAB_CONTENTS_HTML_DIALOG)) {
    // The state is empty and the url will be rendered by WebKit. An empty
    // state is treated as a new navigation by WebKit, which would mean
    // losing the navigation entries and generating a new navigation
    // entry after this one. We don't want that. To avoid this we create
    // a valid state which WebKit will not treat as a new navigation.
    entry->SetContentState(
      webkit_glue::CreateHistoryStateForURL(entry->GetURL()));
  }
}

void NavigationControllerBase::DidNavigateToEntry(NavigationEntry* entry) {
  SetContentStateIfEmpty(entry);

  entry->set_restored(false);

  // If the entry is that of a page with PageID larger than any this Tab has
  // seen before, then consider it a new navigation.  Note that if the entry
  // has a SiteInstance, it should be the same as the SiteInstance of the
  // active WebContents, because we have just navigated to it.
  if (entry->GetPageID() > GetMaxPageID()) {
    InsertEntry(entry);
    NotifyNavigationEntryCommitted();
    return;
  }

  // Otherwise, we just need to update an existing entry with matching PageID.
  // If the existing entry corresponds to the entry which is pending, then we
  // must update the current entry index accordingly.  When navigating to the
  // same URL, a new PageID is not created.

  int existing_entry_index = GetEntryIndexWithPageID(entry->GetType(),
                                                     entry->site_instance(),
                                                     entry->GetPageID());
  NavigationEntry* existing_entry =
      (existing_entry_index != -1) ? entries_[existing_entry_index] : NULL;
  if (!existing_entry) {
    // No existing entry, then simply ignore this navigation!
    DLOG(WARNING) << "ignoring navigation for page: " << entry->GetPageID();
  } else if ((existing_entry != pending_entry_) && pending_entry_ &&
             (pending_entry_->GetPageID() == -1) &&
             (pending_entry_->GetURL() == existing_entry->GetURL())) {
    // Not a new navigation.
    existing_entry->set_unique_id(pending_entry_->unique_id());
    DiscardPendingEntry();
  } else {
    DCHECK(existing_entry != entry);
    // The given entry might provide a new URL... e.g., navigating back to a
    // page in session history could have resulted in a new client redirect.
    // The given entry might also provide a new title (typically an empty title
    // to overwrite the existing title).
    existing_entry->SetURL(entry->GetURL());
    existing_entry->SetTitle(entry->GetTitle());
    existing_entry->SetFavIconURL(entry->GetFavIconURL());
    existing_entry->SetFavIcon(entry->GetFavIcon());
    existing_entry->SetValidFavIcon(entry->IsValidFavIcon());
    existing_entry->SetContentState(entry->GetContentState());

    // TODO(brettw) why only copy the security style and no other SSL stuff?
    existing_entry->ssl().set_security_style(entry->ssl().security_style());

    const int prev_entry_index = last_committed_entry_index_;
    if (existing_entry == pending_entry_) {
      DCHECK(pending_entry_index_ != -1);
      last_committed_entry_index_ = pending_entry_index_;
      // TODO(pkasting): http://b/1113085 Should this use DiscardPendingEntry()?
      DiscardPendingEntryInternal();
    } else {
      // NOTE: Do not update the unique ID here, as we don't want infobars etc.
      // to dismiss.

      // The navigation could have been issued by the renderer, so be sure that
      // we update our current index.
      last_committed_entry_index_ = existing_entry_index;
    }
    IndexOfActiveEntryChanged(prev_entry_index);
  }

  delete entry;

  NotifyNavigationEntryCommitted();
}

void NavigationControllerBase::DiscardPendingEntry() {
  DiscardPendingEntryInternal();

  // Derived classes may do additional things in this case.
}

int NavigationControllerBase::GetIndexOfEntry(
    const NavigationEntry* entry) const {
  const NavigationEntries::const_iterator i(std::find(entries_.begin(),
                                                      entries_.end(), entry));
  return (i == entries_.end()) ? -1 : static_cast<int>(i - entries_.begin());
}

void NavigationControllerBase::DiscardPendingEntryInternal() {
  if (pending_entry_index_ == -1)
    delete pending_entry_;
  pending_entry_ = NULL;
  pending_entry_index_ = -1;
}

void NavigationControllerBase::InsertEntry(NavigationEntry* entry) {
  DCHECK(entry->GetTransitionType() != PageTransition::AUTO_SUBFRAME);

  // Copy the pending entry's unique ID to the committed entry.
  // I don't know if pending_entry_index_ can be other than -1 here.
  const NavigationEntry* const pending_entry = (pending_entry_index_ == -1) ?
      pending_entry_ : entries_[pending_entry_index_];
  if (pending_entry)
    entry->set_unique_id(pending_entry->unique_id());

  DiscardPendingEntryInternal();

  int current_size = static_cast<int>(entries_.size());

  // Prune any entries which are in front of the current entry.
  if (current_size > 0) {
    bool pruned = false;
    while (last_committed_entry_index_ < (current_size - 1)) {
      pruned = true;
      delete entries_[current_size - 1];
      entries_.pop_back();
      current_size--;
    }
    if (pruned)  // Only notify if we did prune something.
      NotifyPrunedEntries();
  }

  if (entries_.size() >= max_entry_count_)
    RemoveEntryAtIndex(0);

  entries_.push_back(entry);
  last_committed_entry_index_ = static_cast<int>(entries_.size()) - 1;
}

void NavigationControllerBase::RemoveLastEntry() {
  int current_size = static_cast<int>(entries_.size());

  if (current_size > 0) {
    if (pending_entry_ == entries_[current_size - 1] ||
        pending_entry_index_ == current_size - 1)
      DiscardPendingEntryInternal();

    delete entries_[current_size - 1];
    entries_.pop_back();

    if (last_committed_entry_index_ >= current_size - 1)
      last_committed_entry_index_ = current_size - 2;

    NotifyPrunedEntries();
  }
}

void NavigationControllerBase::RemoveEntryAtIndex(int index) {
  // TODO(brettw) this is only called to remove the first one when we've got
  // too many entries. It should probably be more specific for this case.
  if (index >= static_cast<int>(entries_.size()) ||
      index == pending_entry_index_ || index == last_committed_entry_index_) {
    NOTREACHED();
    return;
  }

  delete entries_[index];
  entries_.erase(entries_.begin() + index);

  if (last_committed_entry_index_ >= index) {
    if (!entries_.empty())
      last_committed_entry_index_--;
    else
      last_committed_entry_index_ = -1;
  }

  // TODO(brettw) bug 1324021: we probably need some notification here so the
  // session service can stay in sync.
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
  int i, c;
  for (i = 1, c = static_cast<int>(entries_.size()); i < c; ++i) {
    DLOG(INFO) << entries_[i]->GetURL().spec();
  }
}

#endif
