// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/views/focus/external_focus_tracker.h"

#include "chrome/views/view.h"
#include "chrome/views/focus/view_storage.h"

namespace views {

ExternalFocusTracker::ExternalFocusTracker(View* parent_view,
                                           FocusManager* focus_manager)
    : focus_manager_(focus_manager),
      parent_view_(parent_view) {
  view_storage_ = ViewStorage::GetSharedInstance();
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

void ExternalFocusTracker::SetFocusManager(FocusManager* focus_manager) {
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

}  // namespace views
