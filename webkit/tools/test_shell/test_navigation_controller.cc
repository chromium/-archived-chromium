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

#include "webkit/tools/test_shell/test_navigation_controller.h"

#include "base/logging.h"
#include "webkit/glue/webhistoryitem.h"
#include "webkit/tools/test_shell/test_shell.h"

// ----------------------------------------------------------------------------
// TestNavigationEntry

TestNavigationEntry::TestNavigationEntry()
    : NavigationEntry(GetTabContentsType()) {
}

TestNavigationEntry::TestNavigationEntry(int page_id,
                                         const GURL& url,
                                         const std::wstring& title,
                                         PageTransition::Type transition,
                                         const std::wstring& target_frame)
    : NavigationEntry(GetTabContentsType(), page_id, url, title, transition),
      target_frame_(target_frame) {
}

TestNavigationEntry::~TestNavigationEntry() {
}

void TestNavigationEntry::SetContentState(const std::string& state) {
  cached_history_item_ = NULL;  // invalidate our cached item
  NavigationEntry::SetContentState(state);
}

WebHistoryItem* TestNavigationEntry::GetHistoryItem() const {
  if (!cached_history_item_) {
    TestShellExtraRequestData* extra_data = 
        new TestShellExtraRequestData(GetPageID(), GetTransition());
    cached_history_item_ =
        WebHistoryItem::Create(GetURL(), GetTitle(), GetContentState(),
                               extra_data);
  }
  return cached_history_item_;
}

// ----------------------------------------------------------------------------
// TestNavigationController

TestNavigationController::TestNavigationController(TestShell* shell)
    : shell_(shell),
      max_page_id_(-1) {
}

TestNavigationController::~TestNavigationController() {
}

void TestNavigationController::Reset() {
  NavigationControllerBase::Reset();
  max_page_id_ = -1;
}

void TestNavigationController::NavigateToPendingEntry(bool reload) {
  // For session history navigations only the pending_entry_index_ is set.
  if (!pending_entry_) {
    DCHECK(pending_entry_index_ != -1); 
    pending_entry_ = entries_[pending_entry_index_];
  }

  if (shell_->Navigate(*pending_entry_, reload)) {
    // Note: this is redundant if navigation completed synchronously because
    // DidNavigateToEntry call this as well.
    NotifyNavigationStateChanged();
  } else {
    DiscardPendingEntry();
  }
}

void TestNavigationController::NotifyNavigationStateChanged() {
  NavigationEntry* entry = GetActiveEntry();
  if (entry)
    max_page_id_ = std::max(max_page_id_, entry->GetPageID());
}
