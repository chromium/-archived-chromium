// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIEWS_TABS_TAB_OVERVIEW_GRID_H_
#define CHROME_BROWSER_VIEWS_TABS_TAB_OVERVIEW_GRID_H_

#include "chrome/browser/tabs/tab_strip_model.h"
#include "chrome/browser/views/tabs/grid.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "views/view.h"

class TabOverviewCell;

// TabOverviewGrid is used to provide a grid view of the contents of a tab
// strip model. Each cell of the grid is a TabOverviewCell.
class TabOverviewGrid : public Grid, public TabStripModelObserver {
 public:

  class Host {
   public:
    // Invoked when the preferred size of the TabOverviewGrid changes. The
    // preferred size changes any the contents of the tab strip changes.
    virtual void TabOverviewGridPreferredSizeChanged() = 0;

    // Invoked when the user selects a cell in the grid.
    virtual void SelectTabContents(TabContents* contents) = 0;

   protected:
    ~Host() {}
  };

  TabOverviewGrid();
  virtual ~TabOverviewGrid();

  // Sets the host.
  void set_host(Host* host) { host_ = host; }

  // The contents of the TabOverviewGrid are driven by that of the model.
  void SetTabStripModel(TabStripModel* tab_strip_model);
  TabStripModel* model() const { return model_; }

  // TabStripModelObserver overrides.
  virtual void TabInsertedAt(TabContents* contents,
                             int index,
                             bool foreground);
  virtual void TabClosingAt(TabContents* contents, int index);
  virtual void TabDetachedAt(TabContents* contents, int index);
  virtual void TabMoved(TabContents* contents,
                        int from_index,
                        int to_index);
  virtual void TabChangedAt(TabContents* contents, int index,
                            bool loading_only);
  virtual void TabStripEmpty();
  // Currently don't care about these as we're not rendering the selection.
  virtual void TabDeselectedAt(TabContents* contents, int index) { }
  virtual void TabSelectedAt(TabContents* old_contents,
                             TabContents* new_contents,
                             int index,
                             bool user_gesture) { }

  // View overrides.
  virtual bool OnMousePressed(const views::MouseEvent& event);
  virtual bool OnMouseDragged(const views::MouseEvent& event);
  virtual void OnMouseReleased(const views::MouseEvent& event, bool canceled);

 private:
  // DragInfo is used when the user presses the mouse on the grid. It indicates
  // where the press occurred and whether the user is dragging the mouse.
  struct DragInfo {
    DragInfo() : index(-1), dragging(false) {}

    // The index the user pressed that mouse at. If -1, the user didn't press
    // on a valid location.
    int index;

    // Has the user started dragging?
    bool dragging;

    // The origin of the click.
    gfx::Point origin;

    // If true, we're moving the tab in the model. This is used to avoid
    // resetting DragInfo when the model changes.
    bool moving_tab;
  };

  void AddListeners();
  void RemoveListeners();

  // Recreates the contents of the grid from that of the model.
  void Recreate();

  // Returns the TabOverviewCell at the specified index.
  TabOverviewCell* GetTabOverviewCellAt(int index);

  // Configures a cell from the model.
  void ConfigureChild(TabOverviewCell* child, int index);

  // Invoked when the count of the model changes. Notifies the host the pref
  // size changed.
  void TabCountChanged();

  // Updates |drag_info_| based on |location|.
  void UpdateDrag(const gfx::Point& location);

  TabStripModel* model_;

  Host* host_;

  DragInfo drag_info_;

  DISALLOW_COPY_AND_ASSIGN(TabOverviewGrid);
};

#endif  // CHROME_BROWSER_VIEWS_TABS_TAB_OVERVIEW_GRID_H_
