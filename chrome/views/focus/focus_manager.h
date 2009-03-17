// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_VIEWS_FOCUS_FOCUS_MANAGER_H_
#define CHROME_VIEWS_FOCUS_FOCUS_MANAGER_H_

#include "base/basictypes.h"

#if defined(OS_WIN)
#include <windows.h>
#endif
#include <vector>
#include <map>

#include "chrome/common/notification_observer.h"
#include "chrome/views/accelerator.h"

// The FocusManager class is used to handle focus traversal, store/restore
// focused views and handle keyboard accelerators.
//
// There are 2 types of focus:
// - the native focus, which is the focus that an HWND has.
// - the view focus, which is the focus that a views::View has.
//
// Each native view must register with their Focus Manager so the focus manager
// gets notified when they are focused (and keeps track of the native focus) and
// as well so that the tab key events can be intercepted.
// They can provide when they register a View that is kept in synch in term of
// focus. This is used in NativeControl for example, where a View wraps an
// actual native window.
// This is already done for you if you subclass the NativeControl class or if
// you use the HWNDView class.
//
// When creating a top window, if it derives from WidgetWin, the
// |has_own_focus_manager| of the Init method lets you specify whether that
// window should have its own focus manager (so focus traversal stays confined
// in that window). If you are not deriving from WidgetWin or one of its
// derived classes (Window, FramelessWindow, ConstrainedWindow), you must
// create a FocusManager when the window is created (it is automatically deleted
// when the window is destroyed).
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

class View;
class RootView;

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
  // - |dont_loop| if true specifies that if there is a loop in the focus
  //   hierarchy, we should keep traversing after the last view of the loop.
  // - |focus_traversable| is set to the focus traversable that should be
  //   traversed if one is found (in which case the call returns NULL).
  // - |focus_traversable_view| is set to the view associated with the
  //   FocusTraversable set in the previous parameter (it is used as the
  //   starting view when looking for the next focusable view).

  virtual View* FindNextFocusableView(View* starting_view,
                                      bool reverse,
                                      Direction direction,
                                      bool dont_loop,
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
  virtual bool ProcessKeyDown(HWND window, UINT message, WPARAM wparam,
                              LPARAM lparam) = 0;
#endif
};

// This interface should be implemented by classes that want to be notified when
// the focus is about to change.  See the Add/RemoveFocusChangeListener methods.
class FocusChangeListener {
 public:
  virtual void FocusWillChange(View* focused_before, View* focused_now) = 0;
};

class FocusManager : public NotificationObserver {
 public:
#if defined(OS_WIN)
  // Creates a FocusManager for the specified window. Top level windows
  // must invoked this when created.
  // The RootView specified should be the top RootView of the window.
  // This also invokes InstallFocusSubclass.
  static FocusManager* CreateFocusManager(HWND window, RootView* root_view);

  // Subclasses the specified window. The subclassed window procedure listens
  // for WM_SETFOCUS notification and keeps the FocusManager's focus owner
  // property in sync.
  // It's not necessary to explicitly invoke Uninstall, it's automatically done
  // when the window is destroyed and Uninstall wasn't invoked.
  static void InstallFocusSubclass(HWND window, View* view);

  // Uninstalls the window subclass installed by InstallFocusSubclass.
  static void UninstallFocusSubclass(HWND window);

  static FocusManager* GetFocusManager(HWND window);

  // Message handlers (for messages received from registered windows).
  // Should return true if the message should be forwarded to the window
  // original proc function, false otherwise.
  bool OnSetFocus(HWND window);
  bool OnNCDestroy(HWND window);
  // OnKeyDown covers WM_KEYDOWN and WM_SYSKEYDOWN.
  bool OnKeyDown(HWND window,
                 UINT message,
                 WPARAM wparam,
                 LPARAM lparam);
  // OnPostActivate is called after WM_ACTIVATE has been propagated to the
  // DefWindowProc.
  bool OnPostActivate(HWND window, int activation_state, int minimized_state);
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

  // Clears the HWND that has the focus by focusing the HWND from the top
  // RootView (so we still get keyboard events).
  // Note that this does not change the currently focused view.
  void ClearHWNDFocus();

#if defined(OS_WIN)
  // Focus the specified |hwnd| without changing the focused view.
  void FocusHWND(HWND hwnd);
#endif

  // Validates the focused view, clearing it if the window it belongs too is not
  // attached to the window hierarchy anymore.
  void ValidateFocusedView();

#if defined(OS_WIN)
  // Returns the view associated with the specified window if any.
  // If |look_in_parents| is true, it goes up the window parents until it find
  // a view.
  static View* GetViewForWindow(HWND window, bool look_in_parents);
#endif

  // Stores and restores the focused view. Used when the window becomes
  // active/inactive.
  void StoreFocusedView();
  void RestoreFocusedView();

  // Clears the stored focused view.
  void ClearStoredFocusedView();

  // Returns the FocusManager of the parent window of the window that is the
  // root of this FocusManager. This is useful with ConstrainedWindows that have
  // their own FocusManager and need to return focus to the browser when closed.
  FocusManager* GetParentFocusManager() const;

  // Register a keyboard accelerator for the specified target.  If an
  // AcceleratorTarget is already registered for that accelerator, it is
  // returned.
  // Note that we are currently limited to accelerators that are either:
  // - a key combination including Ctrl or Alt
  // - the escape key
  // - the enter key
  // - any F key (F1, F2, F3 ...)
  // - any browser specific keys (as available on special keyboards)
  AcceleratorTarget* RegisterAccelerator(const Accelerator& accelerator,
                                         AcceleratorTarget* target);

  // Unregister the specified keyboard accelerator for the specified target.
  void UnregisterAccelerator(const Accelerator& accelerator,
                             AcceleratorTarget* target);

  // Unregister all keyboard accelerator for the specified target.
  void UnregisterAccelerators(AcceleratorTarget* target);

  // Activate the target associated with the specified accelerator if any.
  // If |prioritary_accelerators_only| is true, only the following accelerators
  // are allowed:
  // - a key combination including Ctrl or Alt
  // - the escape key
  // - the enter key
  // - any F key (F1, F2, F3 ...)
  // - any browser specific keys (as available on special keyboards)
  // Returns true if an accelerator was activated.
  bool ProcessAccelerator(const Accelerator& accelerator,
                          bool prioritary_accelerators_only);

  // NotificationObserver method.
  void Observe(NotificationType type,
               const NotificationSource& source,
               const NotificationDetails& details);

  void AddKeystrokeListener(KeystrokeListener* listener);
  void RemoveKeystrokeListener(KeystrokeListener* listener);

  // Adds/removes a listener.  The FocusChangeListener is notified every time
  // the focused view is about to change.
  void AddFocusChangeListener(FocusChangeListener* listener);
  void RemoveFocusChangeListener(FocusChangeListener* listener);

  // Returns the AcceleratorTarget that should be activated for the specified
  // keyboard accelerator, or NULL if no view is registered for that keyboard
  // accelerator.
  // TODO(finnur): http://b/1307173 Make this private once the bug is fixed.
  AcceleratorTarget* GetTargetForAccelerator(
      const Accelerator& accelerator) const;

 private:
#if defined(OS_WIN)
  explicit FocusManager(HWND root, RootView* root_view);
#endif
  ~FocusManager();

  // Returns the next focusable view.
  View* GetNextFocusableView(View* starting_view, bool reverse, bool dont_loop);

  // Returns the last view of the focus traversal hierarchy.
  View* FindLastFocusableView();

  // Returns the focusable view found in the FocusTraversable specified starting
  // at the specified view. This traverses down along the FocusTraversable
  // hierarchy.
  // Returns NULL if no focusable view were found.
  View* FindFocusableView(FocusTraversable* focus_traversable,
                          View* starting_view,
                          bool reverse,
                          bool dont_loop);

  // The RootView of the window associated with this FocusManager.
  RootView* top_root_view_;

  // The view that currently is focused.
  View* focused_view_;

  // The storage id used in the ViewStorage to store/restore the view that last
  // had focus.
  int stored_focused_view_storage_id_;

#if defined(OS_WIN)
  // The window associated with this focus manager.
  HWND root_;
#endif

  // Used to allow setting the focus on an HWND without changing the currently
  // focused view.
  bool ignore_set_focus_msg_;

  // The accelerators and associated targets.
  typedef std::map<Accelerator, AcceleratorTarget*> AcceleratorMap;
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

#endif  // CHROME_VIEWS_FOCUS_MANAGER_H_
