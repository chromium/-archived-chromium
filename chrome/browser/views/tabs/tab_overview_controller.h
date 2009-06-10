// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIEWS_TABS_TAB_OVERVIEW_CONTROLLER_H_
#define CHROME_BROWSER_VIEWS_TABS_TAB_OVERVIEW_CONTROLLER_H_

#include "base/gfx/rect.h"
#include "base/scoped_ptr.h"
#include "chrome/browser/tabs/tab_strip_model.h"

class Browser;
class TabOverviewCell;
class TabOverviewContainer;
class TabOverviewGrid;

namespace views {
class Widget;
}

// TabOverviewController is responsible for showing a TabOverviewGrid and
// keeping it in sync with the TabStripModel of a browser.
class TabOverviewController : public TabStripModelObserver {
 public:
  // Creates a TabOverviewController that will be shown on the monitor
  // containing |monitor_origin|.
  explicit TabOverviewController(const gfx::Point& monitor_origin);
  ~TabOverviewController();

  // Sets the browser we're showing the tab strip for.
  void SetBrowser(Browser* browser);
  Browser* browser() const { return browser_; }
  TabOverviewGrid* grid() const { return grid_; }
  TabStripModel* model() const;

  // Returns true if the grid has been moved off screen. The grid is moved
  // offscren if the user detaches the last tab in the tab strip.
  bool moved_offscreen() const { return moved_offscreen_; }

  // Shows the grid.
  void Show();

  // Configures a cell from the model.
  void ConfigureCell(TabOverviewCell* cell, TabContents* contents);

  // Invoked from TabOverviewDragController.
  virtual void DragStarted();
  virtual void DragEnded();
  virtual void MoveOffscreen();
  virtual void SelectTabContents(TabContents* contents);

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

 private:
  // Configures a cell from the model.
  void ConfigureCell(TabOverviewCell* cell, int index);

  // Removes all the cells in the grid and populates it from the model.
  void RecreateCells();

  // Invoked when the count of the model changes. Notifies the host the pref
  // size changed.
  void TabCountChanged();

  // The widget showing the view.
  views::Widget* host_;

  // Bounds of the monitor we're being displayed on. This is used to position
  // the widget.
  gfx::Rect monitor_bounds_;

  // View containing the grid, owned by host.
  TabOverviewContainer* container_;

  // The view. This is owned by host.
  TabOverviewGrid* grid_;

  // The browser, not owned by us.
  Browser* browser_;

  // The browser a drag was started on.
  Browser* drag_browser_;

  // True if the host has been moved offscreen.
  bool moved_offscreen_;

  // Has Show been invoked?
  bool shown_;

  DISALLOW_COPY_AND_ASSIGN(TabOverviewController);
};

#endif  // CHROME_BROWSER_VIEWS_TABS_TAB_OVERVIEW_CONTROLLER_H_
