// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/tabs/tab_overview_grid.h"

#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/views/tabs/tab_overview_cell.h"
#include "views/widget/root_view.h"

TabOverviewGrid::TabOverviewGrid()
    : model_(NULL),
      host_(NULL) {
}

TabOverviewGrid::~TabOverviewGrid() {
  RemoveListeners();
  model_ = NULL;
}

void TabOverviewGrid::SetTabStripModel(TabStripModel* tab_strip_model) {
  if (tab_strip_model == model_)
    return;

  RemoveListeners();
  model_ = tab_strip_model;
  AddListeners();
  Recreate();
}

void TabOverviewGrid::TabInsertedAt(TabContents* contents,
                                    int index,
                                    bool foreground) {
  drag_info_ = DragInfo();

  TabOverviewCell* child = new TabOverviewCell();
  ConfigureChild(child, index);
  InsertCell(index, child);

  TabCountChanged();
}

void TabOverviewGrid::TabClosingAt(TabContents* contents, int index) {
  // Nothing to do, we only care when the tab is actually detached.
}

void TabOverviewGrid::TabDetachedAt(TabContents* contents, int index) {
  drag_info_ = DragInfo();

  scoped_ptr<TabOverviewCell> child(GetTabOverviewCellAt(index));
  RemoveCell(index);

  TabCountChanged();
}

void TabOverviewGrid::TabMoved(TabContents* contents,
                               int from_index,
                               int to_index) {
  if (!drag_info_.moving_tab)
    drag_info_ = DragInfo();

  MoveCell(from_index, to_index);
}

void TabOverviewGrid::TabChangedAt(TabContents* contents, int index,
                                   bool loading_only) {
  ConfigureChild(GetTabOverviewCellAt(index), index);
}

void TabOverviewGrid::TabStripEmpty() {
  SetTabStripModel(NULL);
}

bool TabOverviewGrid::OnMousePressed(const views::MouseEvent& event) {
  drag_info_ = DragInfo();
  TabOverviewCell* cell = NULL;
  for (int i = 0; i < GetChildViewCount(); ++i) {
    View* child = GetChildViewAt(i);
    if (child->bounds().Contains(event.location())) {
      drag_info_.index = i;
      cell = static_cast<TabOverviewCell*>(child);
      break;
    }
  }
  if (!cell)
    return false;
  gfx::Point cell_point(event.location());
  ConvertPointToView(this, cell, &cell_point);
  if (!cell->IsPointInThumbnail(cell_point))
    return false;

  drag_info_.origin = event.location();
  return true;
}

bool TabOverviewGrid::OnMouseDragged(const views::MouseEvent& event) {
  if (drag_info_.index >= 0 && !drag_info_.dragging &&
      ExceededDragThreshold(event.location().x() - drag_info_.origin.x(),
                            event.location().y() - drag_info_.origin.y())) {
    // Start dragging.
    drag_info_.dragging = true;
  }
  if (drag_info_.dragging)
    UpdateDrag(event.location());
  return true;
}

void TabOverviewGrid::OnMouseReleased(const views::MouseEvent& event,
                                      bool canceled) {
  if (host_ && !drag_info_.dragging && drag_info_.index != -1)
    host_->SelectTabContents(model_->GetTabContentsAt(drag_info_.index));
}

void TabOverviewGrid::AddListeners() {
  if (model_)
    model_->AddObserver(this);
}

void TabOverviewGrid::RemoveListeners() {
  if (model_)
    model_->RemoveObserver(this);
}

void TabOverviewGrid::Recreate() {
  RemoveAllChildViews(true);

  if (model_) {
    for (int i = 0; i < model_->count(); ++i) {
      TabOverviewCell* child = new TabOverviewCell();
      ConfigureChild(child, i);
      AddChildView(child);
    }
  }
  TabCountChanged();
}

TabOverviewCell* TabOverviewGrid::GetTabOverviewCellAt(int index) {
  return static_cast<TabOverviewCell*>(GetChildViewAt(index));
}

void TabOverviewGrid::ConfigureChild(TabOverviewCell* child, int index) {
  // TODO: need to set thumbnail here.
  TabContents* tab_contents = model_->GetTabContentsAt(index);
  if (tab_contents) {
    child->SetTitle(tab_contents->GetTitle());
    child->SetFavIcon(tab_contents->GetFavIcon());
    child->SchedulePaint();
  } else {
    // Need to figure out under what circumstances this is null and deal.
    NOTIMPLEMENTED();
  }
}

void TabOverviewGrid::TabCountChanged() {
  if (host_)
    host_->TabOverviewGridPreferredSizeChanged();
}

void TabOverviewGrid::UpdateDrag(const gfx::Point& location) {
  int row = 0;
  int col = 0;
  gfx::Rect local_bounds = GetLocalBounds(true);
  if (!local_bounds.Contains(location)) {
    // Local bounds doesn't contain the point, allow dragging to the left/right
    // of us to equal to before/after last cell.
    views::RootView* root = GetRootView();
    gfx::Point root_point = location;
    views::View::ConvertPointToView(this, root, &root_point);
    gfx::Rect root_bounds = root->GetLocalBounds(true);
    if (!root->bounds().Contains(root_point)) {
      // The user dragged outside the grid, remove the cell.
      return;
    }
    if (location.x() < 0)
      col = 0;
    else if (location.x() >= width())
      col = columns();
    else
      col = (location.x() + cell_width() / 2) / (cell_width() + kCellXPadding);
    if (location.y() < 0)
      row = 0;
    else if (location.y() >= height())
      row = rows();
    else
      row = location.y() / (cell_height() + kCellYPadding);
  } else {
    // We contain the point in our bounds.
    col = (location.x() + cell_width() / 2) / (cell_width() + kCellXPadding);
    row = location.y() / (cell_height() + kCellYPadding);
  }
  int new_index = std::min(model_->count() - 1, row * columns() + col);
  if (new_index == drag_info_.index)
    return;
  drag_info_.moving_tab = true;
  model_->MoveTabContentsAt(drag_info_.index, new_index, false);
  drag_info_.moving_tab = false;
  drag_info_.index = new_index;

  // TODO: need to handle dragging outside of window.
}
