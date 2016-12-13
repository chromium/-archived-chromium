// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GTK_TABS_DRAGGED_TAB_CONTROLLER_GTK_H_
#define CHROME_BROWSER_GTK_TABS_DRAGGED_TAB_CONTROLLER_GTK_H_

#include <gtk/gtk.h>

#include <set>

#include "base/scoped_ptr.h"
#include "base/timer.h"
#include "chrome/browser/dock_info.h"
#include "chrome/browser/tab_contents/tab_contents_delegate.h"
#include "chrome/common/notification_registrar.h"
#include "chrome/common/x11_util.h"

class DraggedTabGtk;
class TabGtk;
class TabStripGtk;

class DraggedTabControllerGtk : public NotificationObserver,
                                public TabContentsDelegate {
 public:
  DraggedTabControllerGtk(TabGtk* source_tab, TabStripGtk* source_tabstrip);
  virtual ~DraggedTabControllerGtk();

  // Capture information needed to be used during a drag session for this
  // controller's associated source Tab and TabStrip. |mouse_offset| is the
  // distance of the mouse pointer from the Tab's origin.
  void CaptureDragInfo(const gfx::Point& mouse_offset);

  // Responds to drag events subsequent to StartDrag. If the mouse moves a
  // sufficient distance before the mouse is released, a drag session is
  // initiated.
  void Drag();

  // Complete the current drag session. If the drag session was canceled
  // because the user pressed Escape or something interrupted it, |canceled|
  // is true so the helper can revert the state to the world before the drag
  // begun. Returns whether the tab has been destroyed.
  bool EndDrag(bool canceled);

  // Retrieve the source tab if the TabContents specified matches the one being
  // dragged by this controller, or NULL if the specified TabContents is not
  // the same as the one being dragged.
  TabGtk* GetDragSourceTabForContents(TabContents* contents) const;

  // Returns true if the specified tab matches the tab being dragged.
  bool IsDragSourceTab(TabGtk* tab) const;

 private:
  // Enumeration of the ways a drag session can end.
  enum EndDragType {
    // Drag session exited normally: the user released the mouse.
    NORMAL,

    // The drag session was canceled (alt-tab during drag, escape ...)
    CANCELED,

    // The tab (NavigationController) was destroyed during the drag.
    TAB_DESTROYED
  };

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

  // Overridden from NotificationObserver:
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  // Initialize the offset used to calculate the position to create windows
  // in |GetWindowCreatePoint|.
  void InitWindowCreatePoint();

  // Returns the point where a detached window should be created given the
  // current mouse position.
  gfx::Point GetWindowCreatePoint() const;

  // Sets the TabContents being dragged with the specified |new_contents|.
  void SetDraggedContents(TabContents* new_contents);

  // Move the DraggedTabView according to the current mouse screen position,
  // potentially updating the source and other TabStrips.
  void ContinueDragging();

  // Handles moving the Tab within a TabStrip as well as updating the View.
  void MoveTab(const gfx::Point& screen_point);

  // Returns the compatible TabStrip that is under the specified point (screen
  // coordinates), or NULL if there is none.
  TabStripGtk* GetTabStripForPoint(const gfx::Point& screen_point);

  // Returns the specified |tabstrip| if it contains the specified point
  // (screen coordinates), NULL if it does not.
  TabStripGtk* GetTabStripIfItContains(TabStripGtk* tabstrip,
                                       const gfx::Point& screen_point) const;

  // Attach the dragged Tab to the specified TabStrip.
  void Attach(TabStripGtk* attached_tabstrip, const gfx::Point& screen_point);

  // Detach the dragged Tab from the current TabStrip.
  void Detach();

  // Converts a screen point to a point relative to the tab strip.
  gfx::Point ConvertScreenPointToTabStripPoint(TabStripGtk* tabstrip,
                                               const gfx::Point& screen_point);

  // Retrieve the bounds of the DraggedTabGtk, relative to the attached
  // TabStrip, given location of the dragged tab in screen coordinates.
  gfx::Rect GetDraggedTabTabStripBounds(const gfx::Point& screen_point);

  // Returns the index where the dragged TabContents should be inserted into
  // the attached TabStripModel given the DraggedTabView's bounds
  // |dragged_bounds| in coordinates relative to the attached TabStrip.
  int GetInsertionIndexForDraggedBounds(const gfx::Rect& dragged_bounds) const;

  // Get the position of the dragged tab relative to the attached tab strip.
  gfx::Point GetDraggedTabPoint(const gfx::Point& screen_point);

  // Finds the Tab within the specified TabStrip that corresponds to the
  // dragged TabContents.
  TabGtk* GetTabMatchingDraggedContents(TabStripGtk* tabstrip) const;

  // Does the work for EndDrag. Returns whether the tab has been destroyed.
  bool EndDragImpl(EndDragType how_end);

  // If the drag was aborted for some reason, this function is called to un-do
  // the changes made during the drag operation.
  void RevertDrag();

  // Finishes the drag operation. Returns true if the drag controller should
  // be destroyed immediately, false otherwise.
  bool CompleteDrag();

  // Create the DraggedTabGtk if it does not yet exist.
  void EnsureDraggedTab();

  // Utility for getting the mouse position in screen coordinates.
  gfx::Point GetCursorScreenPoint() const;

  // Gets the screen bounds of a tab.
  static gfx::Rect GetTabScreenBounds(TabGtk* tab);

  // Utility to convert the specified TabStripModel index to something valid
  // for the attached TabStrip.
  int NormalizeIndexToAttachedTabStrip(int index) const;

  // Hides the frame for the window that contains the TabStrip the current
  // drag session was initiated from.
  void HideFrame();

  // Closes a hidden frame at the end of a drag session.
  void CleanUpHiddenFrame();

  // Cleans up a source tab that is no longer used.
  void CleanUpSourceTab();

  // Completes the drag session after the view has animated to its final
  // position.
  void OnAnimateToBoundsComplete();

  // Activates whichever window is under the mouse.
  void BringWindowUnderMouseToFront();

  // Handles registering for notifications.
  NotificationRegistrar registrar_;

  // The TabContents being dragged.
  TabContents* dragged_contents_;

  // The original TabContentsDelegate of |dragged_contents_|, before it was
  // detached from the browser window. We store this so that we can forward
  // certain delegate notifications back to it if we can't handle them locally.
  TabContentsDelegate* original_delegate_;

  // The tab that initiated the drag session.
  TabGtk* source_tab_;

  // The tab strip |source_tab_| originated from.
  TabStripGtk* source_tabstrip_;

  // This is the index of the |source_tab_| in |source_tabstrip_| when the drag
  // began. This is used to restore the previous state if the drag is aborted.
  int source_model_index_;

  // The TabStrip the dragged Tab is currently attached to, or NULL if the
  // dragged Tab is detached.
  TabStripGtk* attached_tabstrip_;

  // The visual representation of the dragged Tab.
  scoped_ptr<DraggedTabGtk> dragged_tab_;

  // The position of the mouse (in screen coordinates) at the start of the drag
  // operation. This is used to calculate minimum elasticity before a
  // DraggedTabView is constructed.
  gfx::Point start_screen_point_;

  // This is the offset of the mouse from the top left of the Tab where
  // dragging begun. This is used to ensure that the dragged view is always
  // positioned at the correct location during the drag, and to ensure that the
  // detached window is created at the right location.
  gfx::Point mouse_offset_;

  // A hint to use when positioning new windows created by detaching Tabs. This
  // is the distance of the mouse from the top left of the dragged tab as if it
  // were the distance of the mouse from the top left of the first tab in the
  // attached TabStrip from the top left of the window.
  gfx::Point window_create_point_;

  // Whether we're in the destructor or not.  Makes sure we don't destroy the
  // drag controller more than once.
  bool in_destructor_;

  // The horizontal position of the mouse cursor in screen coordinates at the
  // time of the last re-order event.
  int last_move_screen_x_;

  // DockInfo for the tabstrip.
  DockInfo dock_info_;

  typedef std::set<GtkWidget*> DockWindows;
  DockWindows dock_windows_;

  // Timer used to bring the window under the cursor to front. If the user
  // stops moving the mouse for a brief time over a browser window, it is
  // brought to front.
  base::OneShotTimer<DraggedTabControllerGtk> bring_to_front_timer_;

  DISALLOW_COPY_AND_ASSIGN(DraggedTabControllerGtk);
};

#endif  // CHROME_BROWSER_GTK_TABS_DRAGGED_TAB_CONTROLLER_GTK_H_
