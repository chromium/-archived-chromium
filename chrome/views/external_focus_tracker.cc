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

#include "chrome/views/external_focus_tracker.h"

#include "chrome/views/view.h"
#include "chrome/views/view_storage.h"

namespace ChromeViews {

ExternalFocusTracker::ExternalFocusTracker(
    ChromeViews::View* parent_view, ChromeViews::FocusManager* focus_manager)
    : focus_manager_(focus_manager),
      parent_view_(parent_view) {
  view_storage_ = ChromeViews::ViewStorage::GetSharedInstance();
  last_focused_view_storage_id_ = view_storage_->CreateStorageID();
  // Store the view which is focused when we're created.
  StartTracking();
}

ExternalFocusTracker::~ExternalFocusTracker() {
  view_storage_->RemoveView(last_focused_view_storage_id_);
  if (focus_manager_)
    focus_manager_->RemoveFocusChangeListener(this);
}

void ExternalFocusTracker::FocusWillChange(View* focused_before,
                                           View* focused_now) {
  if (focused_now && !parent_view_->IsParentOf(focused_now) &&
      parent_view_ != focused_now) {
    // Store the newly focused view.
    StoreLastFocusedView(focused_now);
  }
}

void ExternalFocusTracker::FocusLastFocusedExternalView() {
  View* last_focused_view =
      view_storage_->RetrieveView(last_focused_view_storage_id_);
  if (last_focused_view)
    last_focused_view->RequestFocus();
}

void ExternalFocusTracker::SetFocusManager(
    ChromeViews::FocusManager* focus_manager) {
  if (focus_manager_)
    focus_manager_->RemoveFocusChangeListener(this);
  focus_manager_ = focus_manager;
  if (focus_manager_)
    StartTracking();
}

void ExternalFocusTracker::StoreLastFocusedView(View* view) {
  view_storage_->RemoveView(last_focused_view_storage_id_);
  // If the view is NULL, remove the last focused view from storage, but don't
  // try to store NULL.
  if (view != NULL)
    view_storage_->StoreView(last_focused_view_storage_id_, view);
}

void ExternalFocusTracker::StartTracking() {
  StoreLastFocusedView(focus_manager_->GetFocusedView());
  focus_manager_->AddFocusChangeListener(this);
}

} // namespace
