// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/tabs/tab_overview_grid.h"

#include "chrome/browser/views/tabs/tab_overview_cell.h"
#include "chrome/browser/views/tabs/tab_overview_controller.h"
#include "chrome/browser/views/tabs/tab_overview_drag_controller.h"

TabOverviewGrid::TabOverviewGrid(TabOverviewController* controller)
    : controller_(controller) {
}

TabOverviewGrid::~TabOverviewGrid() {
}

bool TabOverviewGrid::modifying_model() const {
  return drag_controller_.get() && drag_controller_->modifying_model();
}

TabOverviewCell* TabOverviewGrid::GetTabOverviewCellAt(int index) {
  return static_cast<TabOverviewCell*>(GetChildViewAt(index));
}

void TabOverviewGrid::CancelDrag() {
  drag_controller_.reset(NULL);
}

bool TabOverviewGrid::OnMousePressed(const views::MouseEvent& event) {
  if (drag_controller_.get())
    return true;

  if (!event.IsLeftMouseButton())
    return false;

  drag_controller_.reset(new TabOverviewDragController(controller_));
  if (!drag_controller_->Configure(event.location())) {
    drag_controller_.reset(NULL);
    return false;
  }
  return true;
}

bool TabOverviewGrid::OnMouseDragged(const views::MouseEvent& event) {
  if (!drag_controller_.get())
    return false;

  drag_controller_->Drag(event.location());
  return true;
}

void TabOverviewGrid::OnMouseReleased(const views::MouseEvent& event,
                                      bool canceled) {
  if (!drag_controller_.get())
    return;

  if (canceled)
    drag_controller_->RevertDrag();
  else
    drag_controller_->CommitDrag(event.location());
  drag_controller_.reset(NULL);
}
