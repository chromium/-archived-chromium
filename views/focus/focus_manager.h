// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VIEWS_FOCUS_FOCUS_MANAGER_H_
#define VIEWS_FOCUS_FOCUS_MANAGER_H_

#if defined(OS_WIN)
#include <windows.h>
#endif
#include <vector>
#include <map>
#include <list>

#include "base/basictypes.h"
#include "base/gfx/native_widget_types.h"
#include "views/accelerator.h"

// The FocusManager class is used to handle focus traversal, store/restore
// focused views and handle keyboard accelerators.
//
// There are 2 types of focus:
// - the native focus, which is the focus that an gfx::NativeView has.
// - the view focus, which is the focus that a views::View has.
//
// Each native view must register with their Focus Manager so the focus manager
// gets notified when they are focused (and keeps track of the native focus) and
// as well so that the tab key events can be intercepted.
// They can provide when they register a View that is kept in synch in term of
// focus. This is used in NativeControl for example, where a View wraps an
// actual native window.
// This is already done for you if you subclass the NativeControl class or if
// you use the NativeViewHost class.
//
// When creating a top window (derived from views::Widget) that is not a child
// window, it creates and owns a FocusManager to manage the focus for itself and
// all its child windows.
//
// The FocusTraversable interface exposes the methods a class should implement
// in order to be able to be focus traversed when tab key is pressed.
// RootViews implement FocusTraversable.
// The FocusManager contains a top FocusTraversable instance, which is the top
// RootView.
//
// If you just use views, then the focus traversal is handled for you by the
// RootView. The default traversal order is the order in which the views have
// been added to their container. You can modify this order by using the View
// method SetNextFocusableView().
//
// If you are embedding a native view containing a nested RootView (for example
// by adding a NativeControl that contains a WidgetWin as its native
// component), then you need to:
// - override the View::GetFocusTraversable() method in your outter component.
//   It should return the RootView of the inner component. This is used when
//   the focus traversal traverse down the focus hierarchy to enter the nested
//   RootView. In the example mentioned above, the NativeControl overrides
//   GetFocusTraversable() and returns hwnd_view_container_->GetRootView().
// - call RootView::SetFocusTraversableParent() on the nested RootView and point
//   it to the outter RootView. This is used when the focus goes out of the
//   nested RootView. In the example:
//   hwnd_view_container_->GetRootView()->SetFocusTraversableParent(
//      native_control->GetRootView());
// - call RootView::SetFocusTraversableParentView() on the nested RootView with
//   the parent view that directly contains the native window. This is needed
//   when traversing up from the nested RootView to know which view to start
//   with when going to the next/previous view.
//   In our example:
//   hwnd_view_container_->GetRootView()->SetFocusTraversableParent(
//      native_control);
//
// Note that FocusTraversable do not have to be RootViews: TabContents is
// FocusTraversable.

namespace views {

class RootView;
class View;
class Widget;

// The FocusTraversable interface is used by components that want to process
// focus traversal events (due to Tab/Shift-Tab key events).
class FocusTraversable {
 public:
  // The direction in which the focus traversal is going.
  // TODO (jcampan): add support for lateral (left, right) focus traversal. The
  // goal is to switch to focusable views on the same level when using the arrow
  // keys (ala Windows: in a dialog box, arrow keys typically move between the
  // dialog OK, Cancel buttons).
  enum Direction {
    UP = 0,
    DOWN
  };

  // Should find the next view that should be focused and return it. If a
  // FocusTraversable is found while searching for the focusable view, NULL
  // should be returned, focus_traversable should be set to the FocusTraversable
  // and focus_traversable_view should be set to the view associated with the
  // FocusTraversable.
  // This call should return NULL if the end of the focus loop is reached.
  // - |starting_view| is the view that should be used as the starting point
  //   when looking for the previous/next view. It may be NULL (in which case
  //   the first/last view should be used depending if normal/reverse).
  // - |reverse| whether we should find the next (reverse is false) or the
  //   previous (reverse is true) view.
  // - |direction| specifies whether we are traversing down (meaning we should
  //   look into child views) or traversing up (don't look at child views).
  // - |check_starting_view| is true if starting_view may obtain the next focus.
  // - |focus_traversable| is set to the focus traversable that should be
  //   traversed if one is found (in which case the call returns NULL).
  // - |focus_traversable_view| is set to the view associated with the
  //   FocusTraversable set in the previous parameter (it is used as the
  //   starting view when looking for the next focusable view).

  virtual View* FindNextFocusableView(View* starting_view,
                                      bool reverse,
                                      Direction direction,
                                      bool check_starting_view,
                                      FocusTraversable** focus_traversable,
                                      View** focus_traversable_view) = 0;

  // Should return the parent FocusTraversable.
  // The top RootView which is the top FocusTraversable returns NULL.
  virtual FocusTraversable* GetFocusTraversableParent() = 0;

  // This should return the View this FocusTraversable belongs to.
  // It is used when walking up the view hierarchy tree to find which view
  // should be used as the starting view for finding the next/previous view.
  virtual View* GetFocusTraversableParentView() = 0;
};

// The KeystrokeListener interface is used by components (such as the
// ExternalTabContainer class) which need a crack at handling all
// keystrokes.
class KeystrokeListener {
 public:
  // If this returns true, then the component handled the keystroke and ate
  // it.
#if defined(OS_WIN)
  virtual bool ProcessKeyStroke(HWND window, UINT message, WPARAM wparam,
                                LPARAM lparam) = 0;
#endif
};

// This interface should be implemented by classes that want to be notified when
// the focus is about to change.  See the Add/RemoveFocusChangeListener methods.
class FocusChangeListener {
 public:
  virtual void FocusWillChange(View* focused_before, View* focused_now) = 0;
};

class FocusManager {
 public:
  explicit FocusManager(Widget* widget);
  ~FocusManager();

#if defined(OS_WIN)
  // OnKeyDown covers WM_KEYDOWN and WM_SYSKEYDOWN.
  bool OnKeyDown(HWND window,
                 UINT message,
                 WPARAM wparam,
                 LPARAM lparam);
  bool OnKeyUp(HWND window,
               UINT message,
               WPARAM wparam,
               LPARAM lparam);
#endif

  // Returns true is the specified is part of the hierarchy of the window
  // associated with this FocusManager.
  bool ContainsView(View* view);

  // Advances the focus (backward if reverse is true).
  void AdvanceFocus(bool reverse);

  // The FocusManager is handling the selected view for the RootView.
  View* GetFocusedView() const { return focused_view_; }
  void SetFocusedView(View* view);

  // Clears the focused view. The window associated with the top root view gets
  // the native focus (so we still get keyboard events).
  void ClearFocus();

  // Validates the focused view, clearing it if the window it belongs too is not
  // attached to the window hierarchy anymore.
  void ValidateFocusedView();

  // Stores and restores the focused view. Used when the window becomes
  // active/inactive.
  void StoreFocusedView();
  void RestoreFocusedView();

  // Clears the stored focused view.
  void ClearStoredFocusedView();

  // Register a keyboard accelerator for the specified target. If multiple
  // targets are registered for an accelerator, a target registered later has
  // higher priority.
  // Note that we are currently limited to accelerators that are either:
  // - a key combination including Ctrl or Alt
  // - the escape key
  // - the enter key
  // - any F key (F1, F2, F3 ...)
  // - any browser specific keys (as available on special keyboards)
  void RegisterAccelerator(const Accelerator& accelerator,
                           AcceleratorTarget* target);

  // Unregister the specified keyboard accelerator for the specified target.
  void UnregisterAccelerator(const Accelerator& accelerator,
                             AcceleratorTarget* target);

  // Unregister all keyboard accelerator for the specified target.
  void UnregisterAccelerators(AcceleratorTarget* target);

  // Activate the target associated with the specified accelerator.
  // First, AcceleratorPressed handler of the most recently registered target
  // is called, and if that handler processes the event (i.e. returns true),
  // this method immediately returns. If not, we do the same thing on the next
  // target, and so on.
  // Returns true if an accelerator was activated.
  bool ProcessAccelerator(const Accelerator& accelerator);

  // Called by a RootView when a view within its hierarchy is removed from its
  // parent. This will only be called by a RootView in a hierarchy of Widgets
  // that this FocusManager is attached to the parent Widget of.
  void ViewRemoved(View* parent, View* removed);

  void AddKeystrokeListener(KeystrokeListener* listener);
  void RemoveKeystrokeListener(KeystrokeListener* listener);

  // Adds/removes a listener.  The FocusChangeListener is notified every time
  // the focused view is about to change.
  void AddFocusChangeListener(FocusChangeListener* listener);
  void RemoveFocusChangeListener(FocusChangeListener* listener);

  // Returns the AcceleratorTarget that should be activated for the specified
  // keyboard accelerator, or NULL if no view is registered for that keyboard
  // accelerator.
  AcceleratorTarget* GetCurrentTargetForAccelerator(
      const Accelerator& accelertor) const;

  // Convenience method that returns true if the passed |key_event| should
  // trigger tab traversal (if it is a TAB key press with or without SHIFT
  // pressed).
  static bool IsTabTraversalKeyEvent(const KeyEvent& key_event);

  // Sets the focus to the specified native view.
  virtual void FocusNativeView(gfx::NativeView native_view);

  // Clears the native view having the focus.
  virtual void ClearNativeFocus();

  // Retrieves the FocusManager associated with the passed native view.
  static FocusManager* GetFocusManagerForNativeView(
      gfx::NativeView native_view);

 private:
  // Returns the next focusable view.
  View* GetNextFocusableView(View* starting_view, bool reverse, bool dont_loop);

  // Returns the focusable view found in the FocusTraversable specified starting
  // at the specified view. This traverses down along the FocusTraversable
  // hierarchy.
  // Returns NULL if no focusable view were found.
  View* FindFocusableView(FocusTraversable* focus_traversable,
                          View* starting_view,
                          bool reverse);

  // The top-level Widget this FocusManager is associated with.
  Widget* widget_;

  // The view that currently is focused.
  View* focused_view_;

  // The storage id used in the ViewStorage to store/restore the view that last
  // had focus.
  int stored_focused_view_storage_id_;

  // The accelerators and associated targets.
  typedef std::list<AcceleratorTarget*> AcceleratorTargetList;
  typedef std::map<Accelerator, AcceleratorTargetList> AcceleratorMap;
  AcceleratorMap accelerators_;

  // The list of registered keystroke listeners
  typedef std::vector<KeystrokeListener*> KeystrokeListenerList;
  KeystrokeListenerList keystroke_listeners_;

  // The list of registered FocusChange listeners.
  typedef std::vector<FocusChangeListener*> FocusChangeListenerList;
  FocusChangeListenerList focus_change_listeners_;

  DISALLOW_COPY_AND_ASSIGN(FocusManager);
};

}  // namespace views

#endif  // VIEWS_FOCUS_FOCUS_MANAGER_H_
