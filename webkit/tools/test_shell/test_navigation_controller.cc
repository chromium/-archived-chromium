// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/tools/test_shell/test_navigation_controller.h"

#include "base/logging.h"
#include "webkit/glue/webhistoryitem.h"
#include "webkit/tools/test_shell/test_shell.h"

// ----------------------------------------------------------------------------
// TestNavigationEntry

TestNavigationEntry::TestNavigationEntry()
    : page_id_(-1) {
}

TestNavigationEntry::TestNavigationEntry(int page_id,
                                         const GURL& url,
                                         const std::wstring& title,
                                         const std::wstring& target_frame)
    : page_id_(page_id),
      url_(url),
      title_(title),
      target_frame_(target_frame) {
}

TestNavigationEntry::~TestNavigationEntry() {
}

void TestNavigationEntry::SetContentState(const std::string& state) {
  cached_history_item_ = NULL;  // invalidate our cached item
  state_ = state;
}

WebHistoryItem* TestNavigationEntry::GetHistoryItem() const {
  if (!cached_history_item_) {
    TestShellExtraRequestData* extra_data =
        new TestShellExtraRequestData(GetPageID());
    cached_history_item_ =
        WebHistoryItem::Create(GetURL(), GetTitle(), GetContentState(),
                               extra_data);
  }
  return cached_history_item_;
}

// ----------------------------------------------------------------------------
// TestNavigationController

TestNavigationController::TestNavigationController(TestShell* shell)
    : pending_entry_(NULL),
      last_committed_entry_index_(-1),
      pending_entry_index_(-1),
      shell_(shell),
      max_page_id_(-1) {
}

TestNavigationController::~TestNavigationController() {
  DiscardPendingEntry();
}

void TestNavigationController::Reset() {
  entries_.clear();
  DiscardPendingEntry();

  last_committed_entry_index_ = -1;
}

void TestNavigationController::Reload() {
  // Base the navigation on where we are now...
  int current_index = GetCurrentEntryIndex();

  // If we are no where, then we can't reload.  TODO(darin): We should add a
  // CanReload method.
  if (current_index == -1)
    return;

  DiscardPendingEntry();

  pending_entry_index_ = current_index;
  NavigateToPendingEntry(true);
}

void TestNavigationController::GoToOffset(int offset) {
  int index = last_committed_entry_index_ + offset;
  if (index < 0 || index >= GetEntryCount())
    return;

  GoToIndex(index);
}

void TestNavigationController::GoToIndex(int index) {
  DCHECK(index >= 0);
  DCHECK(index < static_cast<int>(entries_.size()));

  DiscardPendingEntry();

  pending_entry_index_ = index;
  NavigateToPendingEntry(false);
}

void TestNavigationController::LoadEntry(TestNavigationEntry* entry) {
  // When navigating to a new page, we don't know for sure if we will actually
  // end up leaving the current page.  The new page load could for example
  // result in a download or a 'no content' response (e.g., a mailto: URL).
  DiscardPendingEntry();
  pending_entry_ = entry;
  NavigateToPendingEntry(false);
}


TestNavigationEntry* TestNavigationController::GetLastCommittedEntry() const {
  if (last_committed_entry_index_ == -1)
    return NULL;
  return entries_[last_committed_entry_index_].get();
}

TestNavigationEntry* TestNavigationController::GetActiveEntry() const {
  TestNavigationEntry* entry = pending_entry_;
  if (!entry)
    entry = GetLastCommittedEntry();
  return entry;
}

int TestNavigationController::GetCurrentEntryIndex() const {
  if (pending_entry_index_ != -1)
    return pending_entry_index_;
  return last_committed_entry_index_;
}


TestNavigationEntry* TestNavigationController::GetEntryAtOffset(
    int offset) const {
  int index = last_committed_entry_index_ + offset;
  if (index < 0 || index >= GetEntryCount())
    return NULL;

  return entries_[index].get();
}

TestNavigationEntry* TestNavigationController::GetEntryWithPageID(
    int32 page_id) const {
  int index = GetEntryIndexWithPageID(page_id);
  return (index != -1) ? entries_[index].get() : NULL;
}

void TestNavigationController::DidNavigateToEntry(TestNavigationEntry* entry) {
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

  int existing_entry_index = GetEntryIndexWithPageID(entry->GetPageID());
  TestNavigationEntry* existing_entry = (existing_entry_index != -1) ?
      entries_[existing_entry_index].get() : NULL;
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
  }

  delete entry;
  UpdateMaxPageID();
}

void TestNavigationController::DiscardPendingEntry() {
  if (pending_entry_index_ == -1)
    delete pending_entry_;
  pending_entry_ = NULL;
  pending_entry_index_ = -1;
}

void TestNavigationController::InsertEntry(TestNavigationEntry* entry) {
  DiscardPendingEntry();

  // Prune any entry which are in front of the current entry
  int current_size = static_cast<int>(entries_.size());
  if (current_size > 0) {
    while (last_committed_entry_index_ < (current_size - 1)) {
      entries_.pop_back();
      current_size--;
    }
  }

  entries_.push_back(linked_ptr<TestNavigationEntry>(entry));
  last_committed_entry_index_ = static_cast<int>(entries_.size()) - 1;
  UpdateMaxPageID();
}

int TestNavigationController::GetEntryIndexWithPageID(int32 page_id) const {
  for (int i = static_cast<int>(entries_.size())-1; i >= 0; --i) {
    if (entries_[i]->GetPageID() == page_id)
      return i;
  }
  return -1;
}

void TestNavigationController::NavigateToPendingEntry(bool reload) {
  // For session history navigations only the pending_entry_index_ is set.
  if (!pending_entry_) {
    DCHECK(pending_entry_index_ != -1);
    pending_entry_ = entries_[pending_entry_index_].get();
  }

  if (shell_->Navigate(*pending_entry_, reload)) {
    // Note: this is redundant if navigation completed synchronously because
    // DidNavigateToEntry call this as well.
    UpdateMaxPageID();
  } else {
    DiscardPendingEntry();
  }
}

void TestNavigationController::UpdateMaxPageID() {
  TestNavigationEntry* entry = GetActiveEntry();
  if (entry)
    max_page_id_ = std::max(max_page_id_, entry->GetPageID());
}
