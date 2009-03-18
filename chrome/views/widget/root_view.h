// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_VIEWS_WIDGET_ROOT_VIEW_H_
#define CHROME_VIEWS_WIDGET_ROOT_VIEW_H_

#include "build/build_config.h"

#if defined(OS_WIN)
#include "base/ref_counted.h"
#endif

#include "chrome/views/focus/focus_manager.h"
#include "chrome/views/view.h"

namespace views {

class PaintTask;
class RootViewDropTarget;
class Widget;

////////////////////////////////////////////////////////////////////////////////
//
// FocusListener Interface
//
////////////////////////////////////////////////////////////////////////////////
class FocusListener {
 public:
  virtual void FocusChanged(View* lost_focus, View* got_focus) = 0;
};


/////////////////////////////////////////////////////////////////////////////
//
// RootView class
//
//   The RootView is the root of a View hierarchy. A RootView is always the
//   first and only child of a Widget.
//
//   The RootView manages the View hierarchy's interface with the Widget
//   and also maintains the current invalid rect - the region that needs
//   repainting.
//
/////////////////////////////////////////////////////////////////////////////
class RootView : public View,
                 public FocusTraversable {
 public:
  static const char kViewClassName[];

  explicit RootView(Widget* widget);

  virtual ~RootView();

  // Layout and Painting functions

  // Overridden from View to implement paint scheduling.
  virtual void SchedulePaint(const gfx::Rect& r, bool urgent);

  // Convenience to schedule the whole view
  virtual void SchedulePaint();

  // Convenience to schedule a paint given some ints
  virtual void SchedulePaint(int x, int y, int w, int h);

  // Paint this RootView and its child Views.
  virtual void ProcessPaint(ChromeCanvas* canvas);

  // If the invalid rect is non-empty and there is a pending paint the RootView
  // is painted immediately. This is internally invoked as the result of
  // invoking SchedulePaint.
  virtual void PaintNow();

  // Whether or not this View needs repainting. If |urgent| is true, this method
  // returns whether this root view needs to paint as soon as possible.
  virtual bool NeedsPainting(bool urgent);

  // Invoked by the Widget to discover what rectangle should be painted.
  const gfx::Rect& GetScheduledPaintRect();

#if defined(OS_WIN)
  // Returns the region scheduled to paint clipped to the RootViews bounds.
  RECT GetScheduledPaintRectConstrainedToSize();
#endif

  // Tree functions

  // Get the Widget that hosts this View.
  virtual Widget* GetWidget() const;

  // Public API for broadcasting theme change notifications to this View
  // hierarchy.
  virtual void ThemeChanged();

  // The following event methods are overridden to propagate event to the
  // control tree
  virtual bool OnMousePressed(const MouseEvent& e);
  virtual bool OnMouseDragged(const MouseEvent& e);
  virtual void OnMouseReleased(const MouseEvent& e, bool canceled);
  virtual void OnMouseMoved(const MouseEvent& e);
  virtual void SetMouseHandler(View* new_mouse_handler);

  // Invoked when the Widget has been fully initialized.
  // At the time the constructor is invoked the Widget may not be completely
  // initialized, when this method is invoked, it is.
  void OnWidgetCreated();

  // Invoked prior to the Widget being destroyed.
  void OnWidgetDestroyed();

  // Invoked By the Widget if the mouse drag is interrupted by
  // the system. Invokes OnMouseReleased with a value of true for canceled.
  void ProcessMouseDragCanceled();

  // Invoked by the Widget instance when the mouse moves outside of the Widget
  // bounds.
  virtual void ProcessOnMouseExited();

  // Make the provided view focused. Also make sure that our Widget is focused.
  void FocusView(View* view);

  // Check whether the provided view is in the focus path. The focus path is the
  // path between the focused view (included) to the root view.
  bool IsInFocusPath(View* view);

  // Returns the View in this RootView hierarchy that has the focus, or NULL if
  // no View currently has the focus.
  View* GetFocusedView();

  // Process a key event. Send the event to the focused view and up the focus
  // path, and finally to the default keyboard handler, until someone consumes
  // it.  Returns whether anyone consumed the event.
  bool ProcessKeyEvent(const KeyEvent& event);

  // Set the default keyboard handler. The default keyboard handler is
  // a view that will get an opportunity to process key events when all
  // views in the focus path did not process an event.
  //
  // Note: this is a single view at this point. We may want to make
  // this a list if needed.
  void SetDefaultKeyboardHandler(View* v);

  // Set whether this root view should focus the corresponding hwnd
  // when an unprocessed mouse event occurs.
  void SetFocusOnMousePressed(bool f);

  // Process a mousewheel event. Return true if the event was processed
  // and false otherwise.
  // MouseWheel events are sent on the focus path.
  virtual bool ProcessMouseWheelEvent(const MouseWheelEvent& e);

  // Overridden to handle special root view case.
  virtual bool IsVisibleInRootView() const;

  // Sets a listener that receives focus changes events.
  void SetFocusListener(FocusListener* listener);

  // FocusTraversable implementation.
  virtual View* FindNextFocusableView(View* starting_view,
                                      bool reverse,
                                      Direction direction,
                                      bool dont_loop,
                                      FocusTraversable** focus_traversable,
                                      View** focus_traversable_view);
  virtual FocusTraversable* GetFocusTraversableParent();
  virtual View* GetFocusTraversableParentView();

  // Used to set the FocusTraversable parent after the view has been created
  // (typically when the hierarchy changes and this RootView is added/removed).
  virtual void SetFocusTraversableParent(FocusTraversable* focus_traversable);

  // Used to set the View parent after the view has been created.
  virtual void SetFocusTraversableParentView(View* view);

  // Returns the name of this class: chrome/views/RootView
  virtual std::string GetClassName() const;

  // Clears the region that is schedule to be painted. You nearly never need
  // to invoke this. This is primarily intended for Widgets.
  void ClearPaintRect();

#if defined(OS_WIN)
  // Invoked from the Widget to service a WM_PAINT call.
  void OnPaint(HWND hwnd);
#endif

#if defined(OS_WIN)
  // Returns the MSAA role of the current view. The role is what assistive
  // technologies (ATs) use to determine what behavior to expect from a given
  // control.
  bool GetAccessibleRole(VARIANT* role);
#endif

  // Returns a brief, identifying string, containing a unique, readable name.
  bool GetAccessibleName(std::wstring* name);

  // Assigns an accessible string name.
  void SetAccessibleName(const std::wstring& name);

 protected:

  // Overridden to properly reset our event propagation member
  // variables when a child is removed
  virtual void ViewHierarchyChanged(bool is_add, View *parent, View *child);

#ifndef NDEBUG
  virtual bool IsProcessingPaint() const { return is_processing_paint_; }
#endif

 private:
  friend class View;
  friend class PaintTask;

  RootView();
  DISALLOW_EVIL_CONSTRUCTORS(RootView);

  // Convert a point to our current mouse handler. Returns false if the
  // mouse handler is not connected to a Widget. In that case, the
  // conversion cannot take place and *p is unchanged
  bool ConvertPointToMouseHandler(const gfx::Point& l, gfx::Point *p);

  // Update the cursor given a mouse event. This is called by non mouse_move
  // event handlers to honor the cursor desired by views located under the
  // cursor during drag operations.
  void UpdateCursor(const MouseEvent& e);

  // Notification that size and/or position of a view has changed. This
  // notifies the appropriate views.
  void ViewBoundsChanged(View* view, bool size_changed, bool position_changed);

  // Registers a view for notification when the visible bounds relative to the
  // root of a view changes.
  void RegisterViewForVisibleBoundsNotification(View* view);
  void UnregisterViewForVisibleBoundsNotification(View* view);

  // Returns the next focusable view or view containing a FocusTraversable (NULL
  // if none was found), starting at the starting_view.
  // skip_starting_view, can_go_up and can_go_down controls the traversal of
  // the views hierarchy.
  // skip_group_id specifies a group_id, -1 means no group. All views from a
  // group are traversed in one pass.
  View* FindNextFocusableViewImpl(View* starting_view,
                                  bool skip_starting_view,
                                  bool can_go_up,
                                  bool can_go_down,
                                  int skip_group_id);

  // Same as FindNextFocusableViewImpl but returns the previous focusable view.
  View* FindPreviousFocusableViewImpl(View* starting_view,
                                      bool skip_starting_view,
                                      bool can_go_up,
                                      bool can_go_down,
                                      int skip_group_id);

  // Convenience method that returns true if a view is focusable and does not
  // belong to the specified group.
  bool IsViewFocusableCandidate(View* v, int skip_group_id);

  // Returns the view selected for the group of the selected view. If the view
  // does not belong to a group or if no view is selected in the group, the
  // specified view is returned.
  static View* FindSelectedViewForGroup(View* view);

  // Updates the last_mouse_* fields from e.
  void SetMouseLocationAndFlags(const MouseEvent& e);

#if defined(OS_WIN)
  // Starts a drag operation for the specified view. This blocks until done.
  // If the view has not been deleted during the drag, OnDragDone is invoked
  // on the view.
  void StartDragForViewFromMouseEvent(View* view,
                                      IDataObject* data,
                                      int operation);
#endif

  // If a view is dragging, this returns it. Otherwise returns NULL.
  View* GetDragView();

  // The view currently handing down - drag - up
  View* mouse_pressed_handler_;

  // The view currently handling enter / exit
  View* mouse_move_handler_;

  // The host Widget
  Widget* widget_;

  // The rectangle that should be painted
  gfx::Rect invalid_rect_;

  // Whether the current invalid rect should be painted urgently.
  bool invalid_rect_urgent_;

  // The task that we are using to trigger some non urgent painting or NULL
  // if no painting has been scheduled yet.
  PaintTask* pending_paint_task_;

  // Indicate if, when the pending_paint_task_ is run, actual painting is still
  // required.
  bool paint_task_needed_;

  // true if mouse_handler_ has been explicitly set
  bool explicit_mouse_handler_;

#if defined(OS_WIN)
  // Previous cursor
  HCURSOR previous_cursor_;
#endif

  // Default keyboard handler
  View* default_keyboard_hander_;

  // The listener that gets focus change notifications.
  FocusListener* focus_listener_;

  // Whether this root view should make our hwnd focused
  // when an unprocessed mouse press event occurs
  bool focus_on_mouse_pressed_;

  // Flag used to ignore focus events when we focus the native window associated
  // with a view.
  bool ignore_set_focus_calls_;

  // Whether this root view belongs to the current active window.
  // bool activated_;

  // Last position/flag of a mouse press/drag. Used if capture stops and we need
  // to synthesize a release.
  int last_mouse_event_flags_;
  int last_mouse_event_x_;
  int last_mouse_event_y_;

  // The parent FocusTraversable, used for focus traversal.
  FocusTraversable* focus_traversable_parent_;

  // The View that contains this RootView. This is used when we have RootView
  // wrapped inside native components, and is used for the focus traversal.
  View* focus_traversable_parent_view_;

#if defined(OS_WIN)
  // Handles dnd for us.
  scoped_refptr<RootViewDropTarget> drop_target_;
#endif

  // Storage of strings needed for accessibility.
  std::wstring accessible_name_;

  // Tracks drag state for a view.
  View::DragInfo drag_info;

  // Valid for the lifetime of StartDragForViewFromMouseEvent, indicates the
  // view the drag started from.
  View* drag_view_;

#ifndef NDEBUG
  // True if we're currently processing paint.
  bool is_processing_paint_;
#endif
};

}  // namespace views

#endif // CHROME_VIEWS_WIDGET_ROOT_VIEW_H_
