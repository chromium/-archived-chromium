// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIEWS_TABS_TAB_OVERVIEW_CONTROLLER_H_
#define CHROME_BROWSER_VIEWS_TABS_TAB_OVERVIEW_CONTROLLER_H_

#include "base/gfx/rect.h"
#include "base/scoped_ptr.h"
#include "base/timer.h"
#include "chrome/browser/tabs/tab_strip_model.h"

class Animation;
class Browser;
class TabOverviewCell;
class TabOverviewContainer;
class TabOverviewGrid;

namespace views {
class Widget;
}

// TabOverviewController is responsible for showing a TabOverviewGrid and
// keeping it in sync with the TabStripModel of a browser.
//
// As tabs are added/removed from the TabStripModel the size and position
// of the container animates. Ideally this would be done by changing the
// bounds of the host window, but that proved janktastic. Instead the
// size of the host window is created at the largest possible size the
// window can be and the bounds of the container are changed during the
// animation.
//
// As obtaining and setting thumbnails is expensive we delay setting the
// thumbnail. The delay is controlled by delay_timer_. Once the timer fires
// another timer is started (configure_timer_). This timer invokes
// ConfigureNextUnconfiguredCell, which obtains and sets the thumbnail of
// the next uncofigured cell. ConfigureNextUnconfiguredCell only configures
// one cell at a time.
class TabOverviewController : public TabStripModelObserver {
 public:
  // Creates a TabOverviewController that will be shown on the monitor
  // containing |monitor_origin|.
  explicit TabOverviewController(const gfx::Point& monitor_origin);
  ~TabOverviewController();

  // Sets the browser we're showing the tab strip for. |horizontal_center|
  // gives the center of the window.
  void SetBrowser(Browser* browser, int horizontal_center);
  Browser* browser() const { return browser_; }
  TabOverviewGrid* grid() const { return grid_; }
  TabStripModel* model() const;

  // Returns true if the grid has been moved off screen. The grid is moved
  // offscren if the user detaches the last tab in the tab strip.
  bool moved_offscreen() const { return moved_offscreen_; }

  // Sets whether the mouse is over a mini-window.
  void SetMouseOverMiniWindow(bool over_mini_window);

  // Shows the grid.
  void Show();

  // Configures a cell from the model.
  void ConfigureCell(TabOverviewCell* cell, TabContents* contents);

  // Invoked from TabOverviewDragController.
  void DragStarted();
  void DragEnded();
  void MoveOffscreen();

  // Selects the TabContents at |index|. This is invoked from the
  // TabOverviewDragController when the user mouses down on a cell.
  void SelectTab(int index);

  // Focuses the current browser. This is invoked from the
  // TabOverviewDragController if the user releases the mouse over a cell
  // without dragging.
  void FocusBrowser();

  // Forwarded from TabOverviewGrid as the animation of the grid changes.
  void GridAnimationEnded();
  void GridAnimationProgressed();
  void GridAnimationCanceled();

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

  // Updates the target and start bounds.
  void UpdateStartAndTargetBounds();

  // Returns the bounds for the tab overview container based on the preferred
  // size of the container. The returned value is the coordinates of the
  // root view (container's parent).
  // See comment above class description for more details.
  gfx::Rect CalculateContainerBounds();

  // Returns the bounds needed for the host.
  // See comment above class description for more details.
  gfx::Rect CalculateHostBounds();

  // Invoked by delay_timer_. Sets show_thumbnails_ to true and starts
  // configure_timer_.
  void StartConfiguring();

  // Finds the first cell with no thumbnail and invokes ConfigureCell for
  // it. If all the thumnbails have been set configure_timer_ is stopped.
  void ConfigureNextUnconfiguredCell();

  // Starts the delay timer.
  void StartDelayTimer();

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

  // Position of the center of the window along the horizontal axis. This is
  // used to position the overview window.
  int horizontal_center_;

  // Should we change the window bounds on animate? This is true while the
  // animation is running on the grid to move things around.
  bool change_window_bounds_on_animate_;

  // When the model changes we animate the bounds of the window. These two
  // give the start and target bounds of the window.
  gfx::Rect start_bounds_;
  gfx::Rect target_bounds_;

  // Are we in the process of mutating the grid? This is used to avoid changing
  // bounds when we're responsible for the mutation.
  bool mutating_grid_;

  // Should we set the thumbnails? This is initially false, then set to true
  // by StartConfiguring.
  bool show_thumbnails_;

  // See description above class for details.
  base::OneShotTimer<TabOverviewController> delay_timer_;

  // See description above class for details.
  base::RepeatingTimer<TabOverviewController> configure_timer_;

  DISALLOW_COPY_AND_ASSIGN(TabOverviewController);
};

#endif  // CHROME_BROWSER_VIEWS_TABS_TAB_OVERVIEW_CONTROLLER_H_
