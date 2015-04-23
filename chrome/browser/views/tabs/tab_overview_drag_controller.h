// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIEWS_TABS_TAB_OVERVIEW_DRAG_CONTROLLER_H_
#define CHROME_BROWSER_VIEWS_TABS_TAB_OVERVIEW_DRAG_CONTROLLER_H_

#include "base/gfx/point.h"
#include "base/scoped_ptr.h"
#include "chrome/browser/tab_contents/tab_contents_delegate.h"
#include "chrome/common/notification_registrar.h"

class Browser;
class TabOverviewController;
class TabOverviewGrid;
class TabStripModel;

namespace views {
class Widget;
}

// TabOverviewDragController handles dragging cells in a TabOverviewGrid.
// There are a couple of interesting states TabOverviewDragController may
// be in:
// . original_index_ == -1: indicates the drag wasn't valid (not over a cell),
//                          or the drag is done (either committed or reverted).
// . detached_tab_ != NULL: the user has dragged a tab outside the grid such
//                          that a windows was created with the contents of
//                          the tab.
// . detached_tab_ == NULL: the user is dragging a cell around within the grid.
//
// TabOverviewGrid invokes Configure to prepare the controller. If this returns
// true, then Drag is repeatedly invoked as the user drags the mouse around.
// Finally CommitDrag is invoked if the user releases the mouse, or RevertDrag
// if the drag is canceled some how.
//
// NOTE: all coordinates passed in are relative to the TabOverviewGrid.
class TabOverviewDragController : public TabContentsDelegate,
                                  public NotificationObserver {
 public:
  explicit TabOverviewDragController(TabOverviewController* controller);
  ~TabOverviewDragController();

  // Sets whether the mouse is over a mini-window.
  void set_mouse_over_mini_window(bool over_mini_window) {
    mouse_over_mini_window_ = over_mini_window;
  }

  // Prepares the TabOverviewDragController for a drag. Returns true if over a
  // cell, false if the mouse isn't over a valid location.
  bool Configure(const gfx::Point& location);

  // Invoked as the user drags the mouse.
  void Drag(const gfx::Point& location);

  // Commits the drag, typically when the user releases the mouse.
  void CommitDrag(const gfx::Point& location);

  // Reverts the drag. Use true if the revert is the result of the tab being
  // destroyed.
  void RevertDrag(bool tab_destroyed);

  bool modifying_model() const { return modifying_model_; }
  TabOverviewGrid* grid() const;
  TabStripModel* model() const;

  // Overridden from NotificationObserver:
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  // Overridden from TabContentsDelegate:
  virtual void OpenURLFromTab(TabContents* source,
                              const GURL& url,
                              const GURL& referrer,
                              WindowOpenDisposition disposition,
                              PageTransition::Type transition);
  virtual void NavigationStateChanged(const TabContents* source,
                                      unsigned changed_flags);
  virtual void AddNewContents(TabContents* source,
                              TabContents* new_contents,
                              WindowOpenDisposition disposition,
                              const gfx::Rect& initial_pos,
                              bool user_gesture);
  virtual void ActivateContents(TabContents* contents);
  virtual void LoadingStateChanged(TabContents* source);
  virtual void CloseContents(TabContents* source);
  virtual void MoveContents(TabContents* source, const gfx::Rect& pos);
  virtual bool IsPopup(TabContents* source);
  virtual void ToolbarSizeChanged(TabContents* source, bool is_animating);
  virtual void URLStarredChanged(TabContents* source, bool starred);
  virtual void UpdateTargetURL(TabContents* source, const GURL& url);

 private:
  // Invoked from Drag if the mouse has moved enough to trigger dragging.
  void DragCell(const gfx::Point& location);

  // Reattaches the detatched tab. |index| is the index into |model()| as to
  // where the tab should be attached.
  void Attach(int index);

  // Detaches the tab at |current_index_|.
  void Detach(const gfx::Point& location);

  // Drops the detached tab. This is invoked from |CommitDrag|.
  void DropTab(const gfx::Point& location);

  // Moves the detached window.
  void MoveDetachedWindow(const gfx::Point& location);

  // Creates and returns the detached window.
  views::Widget* CreateDetachedWindow(const gfx::Point& location,
                                      TabContents* contents);

  // Sets the detaches contents, installed/uninstalling listeners.
  void SetDetachedContents(TabContents* tab);

  TabOverviewController* controller_;

  // The model the drag started from. This needs to be cached as the grid may
  // end up showing a different model if the user drags over another window.
  TabStripModel* original_model_;

  // The index the tab has been dragged to. This is initially the index the
  // user pressed the mouse at, but changes as the user drags the tab around.
  int current_index_;

  // The original index the tab was at. If -1 it means the drag is invalid or
  // done. See description above class for more details.
  int original_index_;

  // The tab being dragged. This is only non-NULL if the tab has been detached.
  TabContents* detached_tab_;

  // If detached_tab_ is non-null, this is it's delegate before we set
  // ourselves as the delegate.
  TabContentsDelegate* original_delegate_;

  // The origin of the click.
  gfx::Point origin_;

  // Offset of the initial mouse location relative to the cell at
  // original_index_.
  int x_offset_;
  int y_offset_;

  // Has the user started dragging?
  bool dragging_;

  // If true, we're modifying the model. This is used to avoid cancelling the
  // drag when the model changes.
  bool modifying_model_;

  // Handles registering for notifications.
  NotificationRegistrar registrar_;

  // Once a tab is detached a window is created containing a cell and moved
  // around; this is that window.
  views::Widget* detached_window_;

  // When a tab is detached from a browser with a single tab we hide the
  // browser. If this is non-null it means a single tab has been detached
  // and this is the browser it was detached from.
  Browser* hidden_browser_;

  // Whether the mouse is over a mini window.
  bool mouse_over_mini_window_;

  // Size of the browser window. Cached in case browser() becomes NULL (as
  // happens when the user drags over a region that shouldn't show the tab
  // overview).
  gfx::Size browser_window_size_;

  DISALLOW_COPY_AND_ASSIGN(TabOverviewDragController);
};

#endif  // CHROME_BROWSER_VIEWS_TABS_TAB_OVERVIEW_DRAG_CONTROLLER_H_
