// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>

#include "base/histogram.h"
#include "base/logging.h"
#include "base/win_util.h"
#include "chrome/browser/renderer_host/render_widget_host_view_win.h"
#include "chrome/common/notification_service.h"
#include "chrome/views/accelerator.h"
#include "chrome/views/focus/focus_manager.h"
#include "chrome/views/focus/view_storage.h"
#include "chrome/views/view.h"
#include "chrome/views/widget/root_view.h"
#include "chrome/views/widget/widget.h"

// The following keys are used in SetProp/GetProp to associate additional
// information needed for focus tracking with a window.

// Maps to the FocusManager instance for a top level window. See
// CreateFocusManager/DestoryFocusManager for usage.
static const wchar_t* const kFocusManagerKey = L"__VIEW_CONTAINER__";

// Maps to the View associated with a window.
// We register views with window so we can:
// - keep in sync the native focus with the view focus (when the native
//   component gets the focus, we get the WM_SETFOCUS event and we can focus the
//   associated view).
// - prevent tab key events from being sent to views.
static const wchar_t* const kViewKey = L"__CHROME_VIEW__";

// A property set to 1 to indicate whether the focus manager has subclassed that
// window.  We are doing this to ensure we are not subclassing several times.
// Subclassing twice is not a problem if no one is subclassing the HWND between
// the 2 subclassings (the 2nd subclassing is ignored since the WinProc is the
// same as the current one).  However if some other app goes and subclasses the
// HWND between the 2 subclassings, we will end up subclassing twice.
// This flag lets us test that whether we have or not subclassed yet.
static const wchar_t* const kFocusSubclassInstalled =
    L"__FOCUS_SUBCLASS_INSTALLED__";

namespace views {

static bool IsCompatibleWithMouseWheelRedirection(HWND window) {
  std::wstring class_name = win_util::GetClassName(window);
  // Mousewheel redirection to comboboxes is a surprising and
  // undesireable user behavior.
  return !(class_name == L"ComboBox" ||
           class_name == L"ComboBoxEx32");
}

static bool CanRedirectMouseWheelFrom(HWND window) {
  std::wstring class_name = win_util::GetClassName(window);

  // Older Thinkpad mouse wheel drivers create a window under mouse wheel
  // pointer. Detect if we are dealing with this window. In this case we
  // don't need to do anything as the Thinkpad mouse driver will send
  // mouse wheel messages to the right window.
  if ((class_name == L"Syn Visual Class") ||
     (class_name == L"SynTrackCursorWindowClass"))
    return false;

  return true;
}

bool IsPluginWindow(HWND window) {
  HWND current_window = window;
  while (GetWindowLong(current_window, GWL_STYLE) & WS_CHILD) {
    current_window = GetParent(current_window);
    if (!IsWindow(current_window))
      break;

    std::wstring class_name = win_util::GetClassName(current_window);
    if (class_name == kRenderWidgetHostHWNDClass)
      return true;
  }

  return false;
}

// Forwards mouse wheel messages to the window under it.
// Windows sends mouse wheel messages to the currently active window.
// This causes a window to scroll even if it is not currently under the
// mouse wheel. The following code gives mouse wheel messages to the
// window under the mouse wheel in order to scroll that window. This
// is arguably a better user experience.  The returns value says whether
// the mouse wheel message was successfully redirected.
static bool RerouteMouseWheel(HWND window, WPARAM wParam, LPARAM lParam) {
  // Since this is called from a subclass for every window, we can get
  // here recursively. This will happen if, for example, a control
  // reflects wheel scroll messages to its parent. Bail out if we got
  // here recursively.
  static bool recursion_break = false;
  if (recursion_break)
    return false;
  // Check if this window's class has a bad interaction with rerouting.
  if (!IsCompatibleWithMouseWheelRedirection(window))
    return false;

  DWORD current_process = GetCurrentProcessId();
  POINT wheel_location = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
  HWND window_under_wheel = WindowFromPoint(wheel_location);

  if (!CanRedirectMouseWheelFrom(window_under_wheel))
    return false;

  // Find the lowest Chrome window in the hierarchy that can be the
  // target of mouse wheel redirection.
  while (window != window_under_wheel) {
    // If window_under_wheel is not a valid Chrome window, then return
    // true to suppress further processing of the message.
    if (!::IsWindow(window_under_wheel))
      return true;
    DWORD wheel_window_process = 0;
    GetWindowThreadProcessId(window_under_wheel, &wheel_window_process);
    if (current_process != wheel_window_process) {
      if (IsChild(window, window_under_wheel)) {
        // If this message is reflected from a child window in a different
        // process (happens with out of process windowed plugins) then
        // we don't want to reroute the wheel message.
        return false;
      } else {
        // The wheel is scrolling over an unrelated window. If that window
        // is a plugin window in a different chrome then we can send it a
        // WM_MOUSEWHEEL. Otherwise, we cannot send random WM_MOUSEWHEEL
        // messages to arbitrary windows. So just drop the message.
        if (!IsPluginWindow(window_under_wheel))
          return true;
      }
    }

    // window_under_wheel is a Chrome window.  If allowed, redirect.
    if (IsCompatibleWithMouseWheelRedirection(window_under_wheel)) {
      recursion_break = true;
      SendMessage(window_under_wheel, WM_MOUSEWHEEL, wParam, lParam);
      recursion_break = false;
      return true;
    }
    // If redirection is disallowed, try the parent.
    window_under_wheel = GetAncestor(window_under_wheel, GA_PARENT);
  }
  // If we traversed back to the starting point, we should process
  // this message normally; return false.
  return false;
}

// Callback installed via InstallFocusSubclass.
static LRESULT CALLBACK FocusWindowCallback(HWND window, UINT message,
                                            WPARAM wParam, LPARAM lParam) {
  if (!::IsWindow(window)) {
    // QEMU has reported crashes when calling GetProp (this seems to happen for
    // some weird messages, not sure what they are).
    // Here we are just trying to avoid the crasher.
    NOTREACHED();
    return 0;
  }

  WNDPROC original_handler = win_util::GetSuperclassWNDPROC(window);
  DCHECK(original_handler);
  FocusManager* focus_manager = FocusManager::GetFocusManager(window);
  // There are cases when we have no FocusManager for the window. This happens
  // because we subclass certain windows (such as the TabContents window)
  // but that window may not have an associated FocusManager.
  if (focus_manager) {
    switch (message) {
      case WM_SETFOCUS:
        if (!focus_manager->OnSetFocus(window))
          return 0;
        break;
      case WM_NCDESTROY:
        if (!focus_manager->OnNCDestroy(window))
          return 0;
        break;
      case WM_ACTIVATE: {
        // We call the DefWindowProc before calling OnActivate as some of our
        // windows need the OnActivate notifications.  The default activation on
        // the window causes it to focus the main window, and since
        // FocusManager::OnActivate attempts to restore the focused view, it
        // needs to be called last so the focus it is setting does not get
        // overridden.
        LRESULT result = CallWindowProc(original_handler, window, WM_ACTIVATE,
                                        wParam, lParam);
        if (!focus_manager->OnPostActivate(window,
                                           LOWORD(wParam), HIWORD(wParam)))
          return 0;
        return result;
      }
      case WM_MOUSEWHEEL:
        if (RerouteMouseWheel(window, wParam, lParam))
          return 0;
        break;
      case WM_IME_CHAR:
        // Issue 7707: A rich-edit control may crash when it receives a
        // WM_IME_CHAR message while it is processing a WM_IME_COMPOSITION
        // message. Since view controls don't need WM_IME_CHAR messages,
        // we prevent WM_IME_CHAR messages from being dispatched to view
        // controls via the CallWindowProc() call.
        return 0;
      default:
        break;
    }
  }
  return CallWindowProc(original_handler, window, message, wParam, lParam);
}

// FocusManager -----------------------------------------------------

// static
FocusManager* FocusManager::CreateFocusManager(HWND window,
                                               RootView* root_view) {
  DCHECK(window);
  DCHECK(root_view);
  InstallFocusSubclass(window, NULL);
  FocusManager* focus_manager = new FocusManager(window, root_view);
  SetProp(window, kFocusManagerKey, focus_manager);

  // We register for view removed notifications so we can make sure we don't
  // keep references to invalidated views.
  NotificationService::current()->AddObserver(
      focus_manager,
      NotificationType::VIEW_REMOVED,
      NotificationService::AllSources());

  return focus_manager;
}

// static
void FocusManager::InstallFocusSubclass(HWND window, View* view) {
  DCHECK(window);

  bool already_subclassed =
      reinterpret_cast<bool>(GetProp(window,
                                     kFocusSubclassInstalled));
  if (already_subclassed &&
      !win_util::IsSubclassed(window, &FocusWindowCallback)) {
    NOTREACHED() << "window sub-classed by someone other than the FocusManager";
    // Track in UMA so we know if this case happens.
    UMA_HISTOGRAM_COUNTS("FocusManager.MultipleSubclass", 1);
  } else {
    win_util::Subclass(window, &FocusWindowCallback);
    SetProp(window, kFocusSubclassInstalled, reinterpret_cast<HANDLE>(true));
  }
  if (view)
    SetProp(window, kViewKey, view);
}

void FocusManager::UninstallFocusSubclass(HWND window) {
  DCHECK(window);
  if (win_util::Unsubclass(window, &FocusWindowCallback)) {
    RemoveProp(window, kViewKey);
    RemoveProp(window, kFocusSubclassInstalled);
  }
}

// static
FocusManager* FocusManager::GetFocusManager(HWND window) {
  DCHECK(window);

  // In case parent windows belong to a different process, yet
  // have the kFocusManagerKey property set, we have to be careful
  // to also check the process id of the window we're checking.
  DWORD window_pid = 0, current_pid = GetCurrentProcessId();
  FocusManager* focus_manager;
  for (focus_manager = NULL; focus_manager == NULL && IsWindow(window);
       window = GetParent(window)) {
    GetWindowThreadProcessId(window, &window_pid);
    if (current_pid != window_pid)
      break;
    focus_manager = reinterpret_cast<FocusManager*>(
        GetProp(window, kFocusManagerKey));
  }
  return focus_manager;
}

// static
View* FocusManager::GetViewForWindow(HWND window, bool look_in_parents) {
  DCHECK(window);
  do {
    View* v = reinterpret_cast<View*>(GetProp(window, kViewKey));
    if (v)
      return v;
  } while (look_in_parents && (window = ::GetParent(window)));

  return NULL;
}

FocusManager::FocusManager(HWND root, RootView* root_view)
    : root_(root),
      top_root_view_(root_view),
      focused_view_(NULL),
      ignore_set_focus_msg_(false) {
  stored_focused_view_storage_id_ =
      ViewStorage::GetSharedInstance()->CreateStorageID();
  DCHECK(root_);
}

FocusManager::~FocusManager() {
  // If there are still registered FocusChange listeners, chances are they were
  // leaked so warn about them.
  DCHECK(focus_change_listeners_.empty());
}

// Message handlers.
bool FocusManager::OnSetFocus(HWND window) {
  if (ignore_set_focus_msg_)
    return true;

  // Focus the view associated with that window.
  View* v = static_cast<View*>(GetProp(window, kViewKey));
  if (v && v->IsFocusable()) {
    v->GetRootView()->FocusView(v);
  } else {
    SetFocusedView(NULL);
  }

  return true;
}

bool FocusManager::OnNCDestroy(HWND window) {
  // Window is being destroyed, undo the subclassing.
  FocusManager::UninstallFocusSubclass(window);

  if (window == root_) {
    // We are the top window.

    DCHECK(GetProp(window, kFocusManagerKey));
    // Unregister notifications.
    NotificationService::current()->RemoveObserver(
        this,
        NotificationType::VIEW_REMOVED,
        NotificationService::AllSources());

    // Make sure this is called on the window that was set with the
    // FocusManager.
    RemoveProp(window, kFocusManagerKey);

    delete this;
  }
  return true;
}

bool FocusManager::OnKeyDown(HWND window, UINT message, WPARAM wparam,
                             LPARAM lparam) {
  DCHECK((message == WM_KEYDOWN) || (message == WM_SYSKEYDOWN));

  if (!IsWindowVisible(root_)) {
    // We got a message for a hidden window. Because WidgetWin::Close hides the
    // window, then destroys it, it it possible to get a message after we've
    // hidden the window. If we allow the message to be dispatched chances are
    // we'll crash in some weird place. By returning false we make sure the
    // message isn't dispatched.
    return false;
  }

  // First give the registered keystoke handlers a chance a processing
  // the message
  // Do some basic checking to try to catch evil listeners that change the list
  // from under us.
  KeystrokeListenerList::size_type original_count =
      keystroke_listeners_.size();
  for (int i = 0; i < static_cast<int>(keystroke_listeners_.size()); i++) {
    if (keystroke_listeners_[i]->ProcessKeyDown(window, message, wparam,
                                                  lparam)) {
      return false;
    }
  }
  DCHECK_EQ(original_count, keystroke_listeners_.size())
      << "KeystrokeListener list modified during notification";

  int virtual_key_code = static_cast<int>(wparam);
  // Intercept Tab related messages for focus traversal.
  // Note that we don't do focus traversal if the root window is not part of the
  // active window hierarchy as this would mean we have no focused view and
  // would focus the first focusable view.
  HWND active_window = ::GetActiveWindow();
  if ((active_window == root_ || ::IsChild(active_window, root_)) &&
      (virtual_key_code == VK_TAB) && !win_util::IsCtrlPressed()) {
    if (!focused_view_ || !focused_view_->CanProcessTabKeyEvents()) {
      AdvanceFocus(win_util::IsShiftPressed());
      return false;
    }
  }

  // Intercept arrow key messages to switch between grouped views.
  if (focused_view_ && focused_view_->GetGroup() != -1 &&
      (virtual_key_code == VK_UP || virtual_key_code == VK_DOWN ||
       virtual_key_code == VK_LEFT || virtual_key_code == VK_RIGHT)) {
    bool next = (virtual_key_code == VK_RIGHT || virtual_key_code == VK_DOWN);
    std::vector<View*> views;
    focused_view_->GetParent()->GetViewsWithGroup(focused_view_->GetGroup(),
                                                  &views);
    std::vector<View*>::const_iterator iter = std::find(views.begin(),
                                                        views.end(),
                                                        focused_view_);
    DCHECK(iter != views.end());
    int index = static_cast<int>(iter - views.begin());
    index += next ? 1 : -1;
    if (index < 0) {
      index = static_cast<int>(views.size()) - 1;
    } else if (index >= static_cast<int>(views.size())) {
      index = 0;
    }
    views[index]->RequestFocus();
    return false;
  }

  int repeat_count = LOWORD(lparam);
  int flags = HIWORD(lparam);
  if (focused_view_ &&
      !focused_view_->ShouldLookupAccelerators(KeyEvent(
          Event::ET_KEY_PRESSED, virtual_key_code,
          repeat_count, flags))) {
    // This should not be processed as an accelerator.
    return true;
  }

  // Process keyboard accelerators.
  // We process accelerators here as we have no way of knowing if a HWND has
  // really processed a key event. If the key combination matches an
  // accelerator, the accelerator is triggered, otherwise we forward the
  // event to the HWND.
  int modifiers = 0;
  if (win_util::IsShiftPressed())
    modifiers |= Event::EF_SHIFT_DOWN;
  if (win_util::IsCtrlPressed())
    modifiers |= Event::EF_CONTROL_DOWN;
  if (win_util::IsAltPressed())
    modifiers |= Event::EF_ALT_DOWN;
  Accelerator accelerator(Accelerator(static_cast<int>(virtual_key_code),
                                      win_util::IsShiftPressed(),
                                      win_util::IsCtrlPressed(),
                                      win_util::IsAltPressed()));
  if (ProcessAccelerator(accelerator, true)) {
    // If a shortcut was activated for this keydown message, do not
    // propagate the message further.
    return false;
  }
  return true;
}

bool FocusManager::OnPostActivate(HWND window,
                                  int activation_state,
                                  int minimized_state) {
  if (WA_INACTIVE == LOWORD(activation_state)) {
    StoreFocusedView();
  } else {
    RestoreFocusedView();
  }
  return false;
}

void FocusManager::ValidateFocusedView() {
  if (focused_view_) {
    if (!ContainsView(focused_view_)) {
      focused_view_ = NULL;
    }
  }
}

// Tests whether a view is valid, whether it still belongs to the window
// hierarchy of the FocusManager.
bool FocusManager::ContainsView(View* view) {
  DCHECK(view);
  RootView* root_view = view->GetRootView();
  if (!root_view)
    return false;

  Widget* widget = root_view->GetWidget();
  if (!widget)
    return false;

  HWND window = widget->GetNativeView();
  while (window) {
    if (window == root_)
      return true;
    window = ::GetParent(window);
  }
  return false;
}

void FocusManager::AdvanceFocus(bool reverse) {
  View* v = GetNextFocusableView(focused_view_, reverse, false);
  // Note: Do not skip this next block when v == focused_view_.  If the user
  // tabs past the last focusable element in a webpage, we'll get here, and if
  // the TabContentsContainerView is the only focusable view (possible in
  // fullscreen mode), we need to run this block in order to cycle around to the
  // first element on the page.
  if (v) {
    v->AboutToRequestFocusFromTabTraversal(reverse);
    v->RequestFocus();
  }
}

View* FocusManager::GetNextFocusableView(View* original_starting_view,
                                         bool reverse,
                                         bool dont_loop) {
  FocusTraversable* focus_traversable = NULL;

  // Let's revalidate the focused view.
  ValidateFocusedView();

  View* starting_view = NULL;
  if (original_starting_view) {
    // If the starting view has a focus traversable, use it.
    // This is the case with WidgetWins for example.
    focus_traversable = original_starting_view->GetFocusTraversable();

    // Otherwise default to the root view.
    if (!focus_traversable) {
      focus_traversable = original_starting_view->GetRootView();
      starting_view = original_starting_view;
    }
  } else {
    focus_traversable = top_root_view_;
  }

  // Traverse the FocusTraversable tree down to find the focusable view.
  View* v = FindFocusableView(focus_traversable, starting_view,
                              reverse, dont_loop);
  if (v) {
    return v;
  } else {
    // Let's go up in the FocusTraversable tree.
    FocusTraversable* parent_focus_traversable =
        focus_traversable->GetFocusTraversableParent();
    starting_view = focus_traversable->GetFocusTraversableParentView();
    while (parent_focus_traversable) {
      FocusTraversable* new_focus_traversable = NULL;
      View* new_starting_view = NULL;
      v = parent_focus_traversable ->FindNextFocusableView(
          starting_view, reverse, FocusTraversable::UP, dont_loop,
          &new_focus_traversable, &new_starting_view);

      if (new_focus_traversable) {
        DCHECK(!v);

        // There is a FocusTraversable, traverse it down.
        v = FindFocusableView(new_focus_traversable, NULL, reverse, dont_loop);
      }

      if (v)
        return v;

      starting_view = focus_traversable->GetFocusTraversableParentView();
      parent_focus_traversable =
          parent_focus_traversable->GetFocusTraversableParent();
    }

    if (!dont_loop) {
      // If we get here, we have reached the end of the focus hierarchy, let's
      // loop.
      if (reverse) {
         // When reversing from the top, the next focusable view is at the end
         // of the focus hierarchy.
        return FindLastFocusableView();
      } else {
        // Easy, just clear the selection and press tab again.
        if (original_starting_view) {  // Make sure there was at least a view to
                                       // start with, to prevent infinitely
                                       // looping in empty windows.
          // By calling with NULL as the starting view, we'll start from the
          // top_root_view.
          return GetNextFocusableView(NULL, false, true);
        }
      }
    }
  }
  return NULL;
}

View* FocusManager::FindLastFocusableView() {
  // Just walk the entire focus loop from where we're at until we reach the end.
  View* new_focused = NULL;
  View* last_focused = focused_view_;
  while (new_focused = GetNextFocusableView(last_focused, false, true))
    last_focused = new_focused;
  return last_focused;
}

void FocusManager::SetFocusedView(View* view) {
  if (focused_view_ != view) {
    View* prev_focused_view = focused_view_;
    if (focused_view_)
      focused_view_->WillLoseFocus();

    if (view)
      view->WillGainFocus();

    // Notified listeners that the focus changed.
    FocusChangeListenerList::const_iterator iter;
    for (iter = focus_change_listeners_.begin();
         iter != focus_change_listeners_.end(); ++iter) {
      (*iter)->FocusWillChange(prev_focused_view, view);
    }

    focused_view_ = view;

    if (prev_focused_view)
      prev_focused_view->SchedulePaint();  // Remove focus artifacts.

    if (view) {
      view->SchedulePaint();
      view->Focus();
      view->DidGainFocus();
    }
  }
}

void FocusManager::ClearFocus() {
  SetFocusedView(NULL);
  ClearHWNDFocus();
}

void FocusManager::ClearHWNDFocus() {
  // Keep the top root window focused so we get keyboard events.
  ignore_set_focus_msg_ = true;
  ::SetFocus(root_);
  ignore_set_focus_msg_ = false;
}

void FocusManager::FocusHWND(HWND hwnd) {
  ignore_set_focus_msg_ = true;
  // Only reset focus if hwnd is not already focused.
  if (hwnd && ::GetFocus() != hwnd)
    ::SetFocus(hwnd);
  ignore_set_focus_msg_ = false;
}

void FocusManager::StoreFocusedView() {
  ViewStorage* view_storage = ViewStorage::GetSharedInstance();
  if (!view_storage) {
    // This should never happen but bug 981648 seems to indicate it could.
    NOTREACHED();
    return;
  }

  // TODO (jcampan): when a WebContents containing a popup is closed, the focus
  // is stored twice causing an assert. We should find a better alternative than
  // removing the view from the storage explicitly.
  view_storage->RemoveView(stored_focused_view_storage_id_);

  if (!focused_view_)
    return;

  view_storage->StoreView(stored_focused_view_storage_id_, focused_view_);

  View* v = focused_view_;
  focused_view_ = NULL;

  if (v)
    v->SchedulePaint();  // Remove focus border.
}

void FocusManager::RestoreFocusedView() {
  ViewStorage* view_storage = ViewStorage::GetSharedInstance();
  if (!view_storage) {
    // This should never happen but bug 981648 seems to indicate it could.
    NOTREACHED();
    return;
  }

  View* view = view_storage->RetrieveView(stored_focused_view_storage_id_);
  if (view) {
    if (ContainsView(view))
      view->RequestFocus();
  } else {
    // Clearing the focus will focus the root window, so we still get key
    // events.
    ClearFocus();
  }
}

void FocusManager::ClearStoredFocusedView() {
  ViewStorage* view_storage = ViewStorage::GetSharedInstance();
  if (!view_storage) {
    // This should never happen but bug 981648 seems to indicate it could.
    NOTREACHED();
    return;
  }
  view_storage->RemoveView(stored_focused_view_storage_id_);
}

FocusManager* FocusManager::GetParentFocusManager() const {
  HWND parent = ::GetParent(root_);
  // If we are a top window, we don't have a parent FocusManager.
  if (!parent)
    return NULL;

  return GetFocusManager(parent);
}

// Find the next (previous if reverse is true) focusable view for the specified
// FocusTraversable, starting at the specified view, traversing down the
// FocusTraversable hierarchy.
View* FocusManager::FindFocusableView(FocusTraversable* focus_traversable,
                                      View* starting_view,
                                      bool reverse,
                                      bool dont_loop) {
  FocusTraversable* new_focus_traversable = NULL;
  View* new_starting_view = NULL;
  View* v = focus_traversable->FindNextFocusableView(starting_view,
                                                     reverse,
                                                     FocusTraversable::DOWN,
                                                     dont_loop,
                                                     &new_focus_traversable,
                                                     &new_starting_view);

  // Let's go down the FocusTraversable tree as much as we can.
  while (new_focus_traversable) {
    DCHECK(!v);
    focus_traversable = new_focus_traversable;
    starting_view = new_starting_view;
    new_focus_traversable = NULL;
    starting_view = NULL;
    v = focus_traversable->FindNextFocusableView(starting_view,
                                                 reverse,
                                                 FocusTraversable::DOWN,
                                                 dont_loop,
                                                 &new_focus_traversable,
                                                 &new_starting_view);
  }
  return v;
}

AcceleratorTarget* FocusManager::RegisterAccelerator(
    const Accelerator& accelerator,
    AcceleratorTarget* target) {
  AcceleratorMap::const_iterator iter = accelerators_.find(accelerator);
  AcceleratorTarget* previous_target = NULL;
  if (iter != accelerators_.end())
    previous_target = iter->second;

  accelerators_[accelerator] = target;

  return previous_target;
}

void FocusManager::UnregisterAccelerator(const Accelerator& accelerator,
                                         AcceleratorTarget* target) {
  AcceleratorMap::iterator iter = accelerators_.find(accelerator);
  if (iter == accelerators_.end()) {
    NOTREACHED() << "Unregistering non-existing accelerator";
    return;
  }

  if (iter->second != target) {
    NOTREACHED() << "Unregistering accelerator for wrong target";
    return;
  }

  accelerators_.erase(iter);
}

void FocusManager::UnregisterAccelerators(AcceleratorTarget* target) {
  for (AcceleratorMap::iterator iter = accelerators_.begin();
       iter != accelerators_.end();) {
    if (iter->second == target)
      accelerators_.erase(iter++);
    else
      ++iter;
  }
}

bool FocusManager::ProcessAccelerator(const Accelerator& accelerator,
                                      bool prioritary_accelerators_only) {
  if (!prioritary_accelerators_only ||
      accelerator.IsCtrlDown() || accelerator.IsAltDown() ||
      accelerator.GetKeyCode() == VK_ESCAPE ||
      accelerator.GetKeyCode() == VK_RETURN ||
      (accelerator.GetKeyCode() >= VK_F1 &&
          accelerator.GetKeyCode() <= VK_F24) ||
      (accelerator.GetKeyCode() >= VK_BROWSER_BACK &&
          accelerator.GetKeyCode() <= VK_BROWSER_HOME)) {
    FocusManager* focus_manager = this;
    do {
      AcceleratorTarget* target =
          focus_manager->GetTargetForAccelerator(accelerator);
      if (target) {
        // If there is focused view, we give it a chance to process that
        // accelerator.
        if (!focused_view_ ||
            !focused_view_->OverrideAccelerator(accelerator)) {
          if (target->AcceleratorPressed(accelerator))
            return true;
        }
      }

      // When dealing with child windows that have their own FocusManager (such
      // as ConstrainedWindow), we still want the parent FocusManager to process
      // the accelerator if the child window did not process it.
      focus_manager = focus_manager->GetParentFocusManager();
    } while (focus_manager);
  }
  return false;
}

AcceleratorTarget* FocusManager::GetTargetForAccelerator(
    const Accelerator& accelerator) const {
  AcceleratorMap::const_iterator iter = accelerators_.find(accelerator);
  if (iter != accelerators_.end())
    return iter->second;
  return NULL;
}

void FocusManager::Observe(NotificationType type,
                           const NotificationSource& source,
                           const NotificationDetails& details) {
  DCHECK(type == NotificationType::VIEW_REMOVED);
  if (focused_view_ && Source<View>(focused_view_) == source)
    focused_view_ = NULL;
}

void FocusManager::AddKeystrokeListener(KeystrokeListener* listener) {
  DCHECK(std::find(keystroke_listeners_.begin(), keystroke_listeners_.end(),
                   listener) == keystroke_listeners_.end())
                       << "Adding a listener twice.";
  keystroke_listeners_.push_back(listener);
}

void FocusManager::RemoveKeystrokeListener(KeystrokeListener* listener) {
  KeystrokeListenerList::iterator place =
      std::find(keystroke_listeners_.begin(), keystroke_listeners_.end(),
                listener);
  if (place == keystroke_listeners_.end()) {
    NOTREACHED() << "Removing a listener that isn't registered.";
    return;
  }
  keystroke_listeners_.erase(place);
}

void FocusManager::AddFocusChangeListener(FocusChangeListener* listener) {
  DCHECK(std::find(focus_change_listeners_.begin(),
                   focus_change_listeners_.end(), listener) ==
      focus_change_listeners_.end()) << "Adding a listener twice.";
  focus_change_listeners_.push_back(listener);
}

void FocusManager::RemoveFocusChangeListener(FocusChangeListener* listener) {
  FocusChangeListenerList::iterator place =
      std::find(focus_change_listeners_.begin(), focus_change_listeners_.end(),
                listener);
  if (place == focus_change_listeners_.end()) {
    NOTREACHED() << "Removing a listener that isn't registered.";
    return;
  }
  focus_change_listeners_.erase(place);
}

}  // namespace views
