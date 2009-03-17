// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/views/widget/root_view_drop_target.h"

#include "base/gfx/point.h"
#include "chrome/common/drag_drop_types.h"
#include "chrome/views/widget/root_view.h"
#include "chrome/views/widget/widget.h"

namespace views {

RootViewDropTarget::RootViewDropTarget(RootView* root_view)
    : BaseDropTarget(root_view->GetWidget()->GetNativeView()),
      root_view_(root_view),
      target_view_(NULL),
      deepest_view_(NULL) {
}

RootViewDropTarget::~RootViewDropTarget() {
}

void RootViewDropTarget::ResetTargetViewIfEquals(View* view) {
  if (target_view_ == view)
    target_view_ = NULL;
  if (deepest_view_ == view)
    deepest_view_ = NULL;
}

DWORD RootViewDropTarget::OnDragOver(IDataObject* data_object,
                                     DWORD key_state,
                                     POINT cursor_position,
                                     DWORD effect) {
  const OSExchangeData data(data_object);
  gfx::Point root_view_location(cursor_position.x, cursor_position.y);
  View::ConvertPointToView(NULL, root_view_, &root_view_location);
  View* view = CalculateTargetView(root_view_location, data);

  if (view != target_view_) {
    // Target changed notify old drag exited, then new drag entered.
    if (target_view_)
      target_view_->OnDragExited();
    target_view_ = view;
    if (target_view_) {
      gfx::Point target_view_location(root_view_location);
      View::ConvertPointToView(root_view_, target_view_, &target_view_location);
      DropTargetEvent enter_event(data,
          target_view_location.x(),
          target_view_location.y(),
          DragDropTypes::DropEffectToDragOperation(effect));
      target_view_->OnDragEntered(enter_event);
    }
  }

  if (target_view_) {
    gfx::Point target_view_location(root_view_location);
    View::ConvertPointToView(root_view_, target_view_, &target_view_location);
    DropTargetEvent enter_event(data,
        target_view_location.x(),
        target_view_location.y(),
        DragDropTypes::DropEffectToDragOperation(effect));
    int result_operation = target_view_->OnDragUpdated(enter_event);
    return DragDropTypes::DragOperationToDropEffect(result_operation);
  } else {
    return DROPEFFECT_NONE;
  }
}

void RootViewDropTarget::OnDragLeave(IDataObject* data_object) {
  if (target_view_)
    target_view_->OnDragExited();
  deepest_view_ = target_view_ = NULL;
}

DWORD RootViewDropTarget::OnDrop(IDataObject* data_object,
                                 DWORD key_state,
                                 POINT cursor_position,
                                 DWORD effect) {
  const OSExchangeData data(data_object);
  DWORD drop_effect = OnDragOver(data_object, key_state, cursor_position,
                                 effect);
  View* drop_view = target_view_;
  deepest_view_ = target_view_ = NULL;
  if (drop_effect != DROPEFFECT_NONE) {
    gfx::Point view_location(cursor_position.x, cursor_position.y);
    View::ConvertPointToView(NULL, drop_view, &view_location);
    DropTargetEvent drop_event(data, view_location.x(), view_location.y(),
        DragDropTypes::DropEffectToDragOperation(effect));
    return DragDropTypes::DragOperationToDropEffect(
        drop_view->OnPerformDrop(drop_event));
  } else {
    if (drop_view)
      drop_view->OnDragExited();
    return DROPEFFECT_NONE;
  }
}

View* RootViewDropTarget::CalculateTargetView(
    const gfx::Point& root_view_location,
    const OSExchangeData& data) {
  View* view = root_view_->GetViewForPoint(root_view_location);
  if (view == deepest_view_) {
    // The view the mouse is over hasn't changed; reuse the target.
    return target_view_;
  }
  // View under mouse changed, which means a new view may want the drop.
  // Walk the tree, stopping at target_view_ as we know it'll accept the
  // drop.
  deepest_view_ = view;
  while (view && view != target_view_ &&
         (!view->IsEnabled() || !view->CanDrop(data))) {
    view = view->GetParent();
  }
  return view;
}

}  // namespace views
