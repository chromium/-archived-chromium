// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GTK_TABS_TAB_STRIP_GTK_H_
#define CHROME_BROWSER_GTK_TABS_TAB_STRIP_GTK_H_

#include <gtk/gtk.h>
#include <vector>

#include "base/basictypes.h"
#include "base/gfx/rect.h"
#include "chrome/browser/gtk/tabs/tab_gtk.h"
#include "chrome/browser/tabs/tab_strip_model.h"
#include "chrome/common/owned_widget_gtk.h"

class CustomDrawButton;
class DraggedTabControllerGtk;

class TabStripGtk : public TabStripModelObserver,
                    public TabGtk::TabDelegate {
 public:
  class TabAnimation;

  explicit TabStripGtk(TabStripModel* model);
  virtual ~TabStripGtk();

  // Initialize and load the TabStrip into a container.
  // TODO(jhawkins): We have to pass in |profile| in order to get a pointer to
  // the theme provider.  In views, all instances of views::View have access to
  // the theme provider.
  void Init(int width, Profile* profile);
  void AddTabStripToBox(GtkWidget* box);

  void Show();
  void Hide();

  TabStripModel* model() const { return model_; }

  // Returns true if there is an active drag session.
  bool IsDragSessionActive() const { return drag_controller_.get() != NULL; }

  // Sets the bounds of the tabs.
  void Layout();

  // Sets the bounds of the tabstrip.
  void SetBounds(const gfx::Rect& bounds);

  // Updates loading animations for the TabStrip.
  void UpdateLoadingAnimations();

  // Returns true if Tabs in this TabStrip are currently changing size or
  // position.
  bool IsAnimating() const;

  // Destroys the active drag controller.
  void DestroyDragController();

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
  virtual void MaybeStartDrag(TabGtk* tab, const gfx::Point& point);
  virtual void ContinueDrag(GdkDragContext* context);
  virtual bool EndDrag(bool canceled);
  virtual bool HasAvailableDragActions() const;

 private:
  friend class DraggedTabControllerGtk;
  friend class InsertTabAnimation;
  friend class RemoveTabAnimation;
  friend class MoveTabAnimation;
  friend class ResizeLayoutAnimation;
  friend class TabAnimation;

  struct TabData {
    TabGtk* tab;
    gfx::Rect ideal_bounds;
  };

  // expose-event handler that redraws the tabstrip
  static gboolean OnExpose(GtkWidget* widget, GdkEventExpose* e,
                           TabStripGtk* tabstrip);

  // size-allocate handler that gets the new bounds of the tabstrip.
  static void OnSizeAllocate(GtkWidget* widget, GtkAllocation* allocation,
                             TabStripGtk* tabstrip);

  // Handles the clicked signal from the new tab button.
  static void OnNewTabClicked(GtkWidget* widget, TabStripGtk* tabstrip);

  // Renders the tabstrip background.
  void PaintBackground(GdkEventExpose* event);

  // Sets the bounds of the tab and moves the tab widget to those bounds.
  void SetTabBounds(TabGtk* tab, const gfx::Rect& bounds);

  // Initializes the new tab button.
  CustomDrawButton* MakeNewTabButton();

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

  // The GtkFixed widget.
  OwnedWidgetGtk tabstrip_;

  // The bounds of the tabstrip.
  gfx::Rect bounds_;

  // Our model.
  TabStripModel* model_;

  // The bitmap we use to paint the colored background.
  SkBitmap* background_;

  // The currently running animation.
  scoped_ptr<TabAnimation> active_animation_;

  // The New Tab button.
  scoped_ptr<CustomDrawButton> newtab_button_;

  // The controller for a drag initiated from a Tab. Valid for the lifetime of
  // the drag session.
  scoped_ptr<DraggedTabControllerGtk> drag_controller_;

  DISALLOW_COPY_AND_ASSIGN(TabStripGtk);
};

#endif  // CHROME_BROWSER_GTK_TABS_TAB_STRIP_GTK_H_
