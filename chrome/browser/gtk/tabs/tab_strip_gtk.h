// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GTK_TABS_TAB_STRIP_GTK_H_
#define CHROME_BROWSER_GTK_TABS_TAB_STRIP_GTK_H_

#include <gtk/gtk.h>
#include <vector>

#include "base/basictypes.h"
#include "base/gfx/rect.h"
#include "chrome/browser/gtk/tabs/tab_button_gtk.h"
#include "chrome/browser/gtk/tabs/tab_gtk.h"
#include "chrome/browser/tabs/tab_strip_model.h"
#include "chrome/common/owned_widget_gtk.h"

class TabStripGtk : public TabStripModelObserver,
                    public TabGtk::TabDelegate,
                    public TabButtonGtk::Delegate {
 public:
  class TabAnimation;

  explicit TabStripGtk(TabStripModel* model);
  virtual ~TabStripGtk();

  // Initialize and load the TabStrip into a container.
  void Init();
  void AddTabStripToBox(GtkWidget* box);

  void Show();
  void Hide();

  TabStripModel* model() const { return model_; }

  // Sets the bounds of the tabs.
  void Layout();

  // Sets the bounds of the tabstrip.
  void SetBounds(const gfx::Rect& bounds) { bounds_ = bounds; }

  // Updates loading animations for the TabStrip.
  void UpdateLoadingAnimations();

  // Returns true if Tabs in this TabStrip are currently changing size or
  // position.
  bool IsAnimating() const;

  // Retrieve the ideal bounds for the Tab at the specified index.
  gfx::Rect GetIdealBounds(int index);

 protected:
  // TabStripModelObserver implementation:
  virtual void TabInsertedAt(TabContents* contents,
                             int index,
                             bool foreground);
  virtual void TabDetachedAt(TabContents* contents, int index);
  virtual void TabSelectedAt(TabContents* old_contents,
                             TabContents* contents,
                             int index,
                             bool user_gesture);
  virtual void TabMoved(TabContents* contents, int from_index, int to_index);
  virtual void TabChangedAt(TabContents* contents, int index,
                            bool loading_only);

  // TabGtk::TabDelegate implementation:
  virtual bool IsTabSelected(const TabGtk* tab) const;
  virtual void SelectTab(TabGtk* tab);
  virtual void CloseTab(TabGtk* tab);
  virtual bool IsCommandEnabledForTab(
      TabStripModel::ContextMenuCommand command_id, const TabGtk* tab) const;
  virtual void ExecuteCommandForTab(
      TabStripModel::ContextMenuCommand command_id, TabGtk* tab);
  virtual void StartHighlightTabsForCommand(
      TabStripModel::ContextMenuCommand command_id, TabGtk* tab);
  virtual void StopHighlightTabsForCommand(
      TabStripModel::ContextMenuCommand command_id, TabGtk* tab);
  virtual void StopAllHighlighting();
  virtual bool EndDrag(bool canceled);
  virtual bool HasAvailableDragActions() const;

  // TabButtonGtk::Delegate implementation:
  virtual GdkRegion* MakeRegionForButton(const TabButtonGtk* button) const;
  virtual void OnButtonActivate(const TabButtonGtk* button);

 private:
  friend class InsertTabAnimation;
  friend class RemoveTabAnimation;
  friend class MoveTabAnimation;
  friend class SnapTabAnimation;
  friend class ResizeLayoutAnimation;
  friend class TabAnimation;

  struct TabData {
    TabGtk* tab;
    gfx::Rect ideal_bounds;
  };

  // expose-event handler that redraws the tabstrip
  static gboolean OnExpose(GtkWidget* widget, GdkEventExpose* e,
                           TabStripGtk* tabstrip);

  // configure-event handler that gets the new bounds of the tabstrip.
  static gboolean OnConfigure(GtkWidget* widget, GdkEventConfigure* event,
                              TabStripGtk* tabstrip);

  // motion-notify-event handler that handles mouse movement in the tabstrip.
  static gboolean OnMotionNotify(GtkWidget* widget, GdkEventMotion* event,
                                 TabStripGtk* tabstrip);

  // button-press-event handler that handles mouse clicks.
  static gboolean OnMousePress(GtkWidget* widget, GdkEventButton* event,
                               TabStripGtk* tabstrip);

  // button-release-event handler that handles mouse click releases.
  static gboolean OnMouseRelease(GtkWidget* widget, GdkEventButton* event,
                                 TabStripGtk* tabstrip);

  // enter-notify-event handler that signals when the mouse enters the tabstrip.
  static gboolean OnEnterNotify(GtkWidget* widget, GdkEventCrossing* event,
                                TabStripGtk* tabstrip);

  // leave-notify-event handler that signals when the mouse leaves the tabstrip.
  static gboolean OnLeaveNotify(GtkWidget* widget, GdkEventCrossing* event,
                                TabStripGtk* tabstrip);

  // drag-begin handler that signals when a drag action begins.
  static void OnDragBegin(GtkWidget* widget, GdkDragContext* context,
                          TabStripGtk* tabstrip);

  // drag-end handler that signals when a drag action ends.
  static void OnDragEnd(GtkWidget* widget, GdkDragContext* context,
                        TabStripGtk* tabstrip);

  // drag-motion handler that handles drag movements in the tabstrip.
  static gboolean OnDragMotion(GtkWidget* widget, GdkDragContext* context,
                               guint x, guint y, guint time,
                               TabStripGtk* tabstrip);

  // drag-failed handler that is emitted when the drag fails.
  static gboolean OnDragFailed(GtkWidget* widget, GdkDragContext* context,
                               GtkDragResult result, TabStripGtk* tabstrip);

  // Finds the tab that is under |point| by iterating through all of the tabs
  // and checking if |point| is in their bounds.  This method is only used when
  // the state of all the tabs cannot be calculated, as during a SnapTab
  // animation.  Runs in O(n) time.
  int FindTabHoverIndexIterative(const gfx::Point& point);

  // Finds the tab that is under |point| by estimating the tab index and
  // checking if |point| is in the bounds of the surrounding tabs.  This method
  // is optimal and is used in most cases.  Runs in O(1) time.
  int FindTabHoverIndexFast(const gfx::Point& point);

  // -- Drag & Drop ------------------------------------------------------------
  //
  // TODO(jhawkins): These functions belong in DraggedTabControllerGtk.

  // Returns the index where the dragged TabContents should be inserted into
  // the attached TabStripModel given the DraggedTabView's bounds
  // |dragged_bounds| in coordinates relative to the attached TabStrip.
  int GetInsertionIndexForDraggedBounds(const gfx::Rect& dragged_bounds);

  // Utility to convert the specified TabStripModel index to something valid
  // for the attached TabStrip.
  int NormalizeIndexToAttachedTabStrip(int index);

  // Handles moving the Tab within a TabStrip.
  void MoveTab(TabGtk* tab, const gfx::Point& point);

  // Get the position of the dragged tab relative to the attached tab strip.
  gfx::Point GetDraggedPoint(TabGtk* tab, const gfx::Point& point);

  // Gets the number of Tabs in the collection.
  int GetTabCount() const;

  // Retrieves the Tab at the specified index.
  TabGtk* GetTabAt(int index) const;

  // Returns the exact (unrounded) current width of each tab.
  void GetCurrentTabWidths(double* unselected_width,
                           double* selected_width) const;

  // Returns the exact (unrounded) desired width of each tab, based on the
  // desired strip width and number of tabs.  If
  // |width_of_tabs_for_mouse_close_| is nonnegative we use that value in
  // calculating the desired strip width; otherwise we use the current width.
  void GetDesiredTabWidths(int tab_count,
                           double* unselected_width,
                           double* selected_width) const;

  // Perform an animated resize-relayout of the TabStrip immediately.
  void ResizeLayoutTabs();

  // Calculates the available width for tabs, assuming a Tab is to be closed.
  int GetAvailableWidthForTabs(TabGtk* last_tab) const;

  // Finds the index of the TabContents corresponding to |tab| in our
  // associated TabStripModel, or -1 if there is none (e.g. the specified |tab|
  // is being animated closed).
  int GetIndexOfTab(const TabGtk* tab) const;

  // Cleans up the tab from the TabStrip at the specified |index|.
  void RemoveTabAt(int index);

  // Generates the ideal bounds of the TabStrip when all Tabs have finished
  // animating to their desired position/bounds. This is used by the standard
  // Layout method and other callers like the DraggedTabController that need
  // stable representations of Tab positions.
  void GenerateIdealBounds();

  // Lays out the New Tab button, assuming the right edge of the last Tab on
  // the TabStrip at |last_tab_right|.  |unselected_width| is the width of
  // unselected tabs at the moment this function is called.  The value changes
  // during animations, so we can't use current_unselected_width_.
  void LayoutNewTabButton(double last_tab_right, double unselected_width);

  // -- Animations -------------------------------------------------------------

  // A generic Layout method for various classes of TabStrip animations,
  // including Insert, Remove and Resize Layout cases.
  void AnimationLayout(double unselected_width);

  // Starts various types of TabStrip animations.
  void StartInsertTabAnimation(int index);
  void StartRemoveTabAnimation(int index, TabContents* contents);
  void StartResizeLayoutAnimation();
  void StartMoveTabAnimation(int from_index, int to_index);
  void StartSnapTabAnimation(const gfx::Rect& bounds);

  // Returns true if detach or select changes in the model should be reflected
  // in the TabStrip. This returns false if we're closing all tabs in the
  // TabStrip and so we should prevent updating. This is not const because we
  // use this as a signal to cancel any active animations.
  bool CanUpdateDisplay();

  // Notifies the TabStrip that the specified TabAnimation has completed.
  // Optionally a full Layout will be performed, specified by |layout|.
  void FinishAnimation(TabAnimation* animation, bool layout);

  // The Tabs we contain, and their last generated "good" bounds.
  std::vector<TabData> tab_data_;

  // The current widths of various types of tabs.  We save these so that, as
  // users close tabs while we're holding them at the same size, we can lay out
  // tabs exactly and eliminate the "pixel jitter" we'd get from just leaving
  // them all at their existing, rounded widths.
  double current_unselected_width_;
  double current_selected_width_;

  // If this value is nonnegative, it is used in GetDesiredTabWidths() to
  // calculate how much space in the tab strip to use for tabs.  Most of the
  // time this will be -1, but while we're handling closing a tab via the mouse,
  // we'll set this to the edge of the last tab before closing, so that if we
  // are closing the last tab and need to resize immediately, we'll resize only
  // back to this width, thus once again placing the last tab under the mouse
  // cursor.
  int available_width_for_tabs_;

  // True if a resize layout animation should be run a short delay after the
  // mouse exits the TabStrip.
  // TODO(beng): (Cleanup) this would be better named "needs_resize_layout_".
  bool resize_layout_scheduled_;

  // The drawing area widget.
  OwnedWidgetGtk tabstrip_;

  // The bounds of the tabstrip.
  gfx::Rect bounds_;

  // Our model.
  TabStripModel* model_;

  // The index of the tab the mouse is currently over.  -1 if not over a tab.
  int hover_index_;

  // The currently running animation.
  scoped_ptr<TabAnimation> active_animation_;

  // The New Tab button.
  scoped_ptr<TabButtonGtk> newtab_button_;

  // ===========================================================================
  // TODO(jhawkins): This belongs in DraggedTabControllerGtk.

  // This is the offset of the mouse from the top left of the Tab where
  // dragging begun. This is used to ensure that the dragged view is always
  // positioned at the correct location during the drag, and to ensure that the
  // detached window is created at the right location.
  gfx::Point mouse_offset_;

  // The horizontal position of the mouse cursor at the time of the last
  // re-order event.
  int last_move_x_;

  // The last good tab bounds of the dragged tab.  This is the position the tab
  // will be snapped back to when the drag is released.
  gfx::Rect snap_bounds_;

  // When a tab is being dragged, certain gtk events should be ignored.
  bool is_dragging_;

  DISALLOW_COPY_AND_ASSIGN(TabStripGtk);
};

#endif  // CHROME_BROWSER_GTK_TABS_TAB_STRIP_GTK_H_
