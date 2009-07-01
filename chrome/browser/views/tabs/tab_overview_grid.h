// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIEWS_TABS_TAB_OVERVIEW_GRID_H_
#define CHROME_BROWSER_VIEWS_TABS_TAB_OVERVIEW_GRID_H_

#include "base/scoped_ptr.h"
#include "chrome/browser/views/tabs/grid.h"

class TabOverviewCell;
class TabOverviewController;
class TabOverviewDragController;

// TabOverviewGrid is used to provide a grid view of the contents of a tab
// strip model. Each cell of the grid is a TabOverviewCell. TabOverviewGrids
// primary responsibility is to forward events to TabOverviewDragController.
class TabOverviewGrid : public Grid {
 public:
  explicit TabOverviewGrid(TabOverviewController* controller);
  virtual ~TabOverviewGrid();

  // Returns true if a drag is underway and the drag is in the process of
  // modifying the tab strip model.
  bool modifying_model() const;

  // Returns the TabOverviewCell at the specified index.
  TabOverviewCell* GetTabOverviewCellAt(int index);

  // Returns the TabOverviewDragController. This is NULL if a drag is not
  // underway.
  TabOverviewDragController* drag_controller() const;

  // Cancels the drag. Does nothing if a drag is not underway.
  void CancelDrag();

  // If a drag is under way, this invokes Drag on the DragController with the
  // current position of the mouse.
  void UpdateDragController();

  // View overrides.
  virtual bool OnMousePressed(const views::MouseEvent& event);
  virtual bool OnMouseDragged(const views::MouseEvent& event);
  virtual void OnMouseReleased(const views::MouseEvent& event, bool canceled);

  // AnimationDelegate overrides.
  virtual void AnimationEnded(const Animation* animation);
  virtual void AnimationProgressed(const Animation* animation);
  virtual void AnimationCanceled(const Animation* animation);

 private:
  TabOverviewController* controller_;

  scoped_ptr<TabOverviewDragController> drag_controller_;

  DISALLOW_COPY_AND_ASSIGN(TabOverviewGrid);
};

#endif  // CHROME_BROWSER_VIEWS_TABS_TAB_OVERVIEW_GRID_H_
