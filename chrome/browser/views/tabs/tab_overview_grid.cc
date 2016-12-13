// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/tabs/tab_overview_grid.h"

#include "chrome/browser/views/tabs/tab_overview_cell.h"
#include "chrome/browser/views/tabs/tab_overview_controller.h"
#include "chrome/browser/views/tabs/tab_overview_drag_controller.h"
#include "views/screen.h"

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

TabOverviewDragController* TabOverviewGrid::drag_controller() const {
  return drag_controller_.get();
}

void TabOverviewGrid::CancelDrag() {
  drag_controller_.reset(NULL);
}

void TabOverviewGrid::UpdateDragController() {
  if (drag_controller_.get()) {
    gfx::Point mouse_loc = views::Screen::GetCursorScreenPoint();
    ConvertPointToView(NULL, this, &mouse_loc);
    drag_controller_->Drag(mouse_loc);
  }
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
    drag_controller_->RevertDrag(false);
  else
    drag_controller_->CommitDrag(event.location());
  drag_controller_.reset(NULL);
}

void TabOverviewGrid::AnimationEnded(const Animation* animation) {
  Grid::AnimationEnded(animation);
  controller_->GridAnimationEnded();
}

void TabOverviewGrid::AnimationProgressed(const Animation* animation) {
  Grid::AnimationProgressed(animation);
  controller_->GridAnimationProgressed();
}

void TabOverviewGrid::AnimationCanceled(const Animation* animation) {
  Grid::AnimationCanceled(animation);
  controller_->GridAnimationCanceled();
}
