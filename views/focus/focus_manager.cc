// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/focus/focus_manager.h"

#include <algorithm>

#include "build/build_config.h"

#if defined(OS_LINUX)
#include <gtk/gtk.h>
#endif

#include "base/histogram.h"
#include "base/logging.h"
#include "views/accelerator.h"
#include "views/focus/view_storage.h"
#include "views/view.h"
#include "views/widget/root_view.h"
#include "views/widget/widget.h"

#if defined(OS_WIN)
#include "base/win_util.h"
#endif

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

#if defined(OS_WIN)
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
      case WM_NCDESTROY:
        if (!focus_manager->OnNCDestroy(window))
          return 0;
        break;
      default:
        break;
    }
  }
  return CallWindowProc(original_handler, window, message, wParam, lParam);
}

#endif

// FocusManager -----------------------------------------------------

#if defined(OS_WIN)
// static
FocusManager* FocusManager::CreateFocusManager(HWND window,
                                               RootView* root_view) {
  DCHECK(window);
  DCHECK(root_view);
  InstallFocusSubclass(window, NULL);
  FocusManager* focus_manager = new FocusManager(window, root_view);
  SetProp(window, kFocusManagerKey, focus_manager);

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

#endif

// static
FocusManager* FocusManager::GetFocusManager(gfx::NativeView window) {
#if defined(OS_WIN)
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
#else
  NOTIMPLEMENTED();
  return NULL;
#endif
}

#if defined(OS_WIN)
// static
View* FocusManager::GetViewForWindow(gfx::NativeView window, bool look_in_parents) {
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
#endif

FocusManager::~FocusManager() {
  // If there are still registered FocusChange listeners, chances are they were
  // leaked so warn about them.
  DCHECK(focus_change_listeners_.empty());
}

#if defined(OS_WIN)
// Message handlers.
bool FocusManager::OnNCDestroy(HWND window) {
  // Window is being destroyed, undo the subclassing.
  FocusManager::UninstallFocusSubclass(window);

  if (window == root_) {
    // We are the top window.

    DCHECK(GetProp(window, kFocusManagerKey));

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

  // First give the registered keystroke handlers a chance a processing
  // the message
  // Do some basic checking to try to catch evil listeners that change the list
  // from under us.
  KeystrokeListenerList::size_type original_count =
      keystroke_listeners_.size();
  for (int i = 0; i < static_cast<int>(keystroke_listeners_.size()); i++) {
    if (keystroke_listeners_[i]->ProcessKeyStroke(window, message, wparam,
                                                  lparam)) {
      return false;
    }
  }
  DCHECK_EQ(original_count, keystroke_listeners_.size())
      << "KeystrokeListener list modified during notification";

  int virtual_key_code = static_cast<int>(wparam);
  int repeat_count = LOWORD(lparam);
  int flags = HIWORD(lparam);
  KeyEvent key_event(Event::ET_KEY_PRESSED,
                     virtual_key_code, repeat_count, flags);

  // If the focused view wants to process the key event as is, let it be.
  if (focused_view_ && focused_view_->SkipDefaultKeyEventProcessing(key_event))
    return true;

  // Intercept Tab related messages for focus traversal.
  // Note that we don't do focus traversal if the root window is not part of the
  // active window hierarchy as this would mean we have no focused view and
  // would focus the first focusable view.
  HWND active_window = ::GetActiveWindow();
  if ((active_window == root_ || ::IsChild(active_window, root_)) &&
      IsTabTraversalKeyEvent(key_event)) {
    AdvanceFocus(win_util::IsShiftPressed());
    return false;
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

  // Process keyboard accelerators.
  // We process accelerators here as we have no way of knowing if a HWND has
  // really processed a key event. If the key combination matches an
  // accelerator, the accelerator is triggered, otherwise we forward the
  // event to the HWND.
  Accelerator accelerator(Accelerator(static_cast<int>(virtual_key_code),
                                      win_util::IsShiftPressed(),
                                      win_util::IsCtrlPressed(),
                                      win_util::IsAltPressed()));
  if (ProcessAccelerator(accelerator)) {
    // If a shortcut was activated for this keydown message, do not propagate
    // the message further.
    return false;
  }
  return true;
}

bool FocusManager::OnKeyUp(HWND window, UINT message, WPARAM wparam,
                           LPARAM lparam) {
  for (int i = 0; i < static_cast<int>(keystroke_listeners_.size()); ++i) {
    if (keystroke_listeners_[i]->ProcessKeyStroke(window, message, wparam,
                                                  lparam)) {
      return false;
    }
  }

  return true;
}
#endif

void FocusManager::ValidateFocusedView() {
  if (focused_view_) {
    if (!ContainsView(focused_view_))
      ClearFocus();
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

  gfx::NativeView window = widget->GetNativeView();
  while (window) {
    if (window == root_)
      return true;
#if defined(OS_WIN)
    window = ::GetParent(window);
#else
    window = gtk_widget_get_parent(window);
#endif
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
  while ((new_focused = GetNextFocusableView(last_focused, false, true)))
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
#if defined(OS_WIN)
  ::SetFocus(root_);
#else
  NOTIMPLEMENTED();
#endif
  ignore_set_focus_msg_ = false;
}

#if defined(OS_WIN)
void FocusManager::FocusHWND(HWND hwnd) {
  ignore_set_focus_msg_ = true;
  // Only reset focus if hwnd is not already focused.
  if (hwnd && ::GetFocus() != hwnd)
    ::SetFocus(hwnd);
  ignore_set_focus_msg_ = false;
}
#endif

void FocusManager::StoreFocusedView() {
  ViewStorage* view_storage = ViewStorage::GetSharedInstance();
  if (!view_storage) {
    // This should never happen but bug 981648 seems to indicate it could.
    NOTREACHED();
    return;
  }

  // TODO (jcampan): when a TabContents containing a popup is closed, the focus
  // is stored twice causing an assert. We should find a better alternative than
  // removing the view from the storage explicitly.
  view_storage->RemoveView(stored_focused_view_storage_id_);

  if (!focused_view_)
    return;

  view_storage->StoreView(stored_focused_view_storage_id_, focused_view_);

  View* v = focused_view_;
  ClearFocus();

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
#if defined(OS_WIN)
  HWND parent = ::GetParent(root_);
#else
  GtkWidget* parent = gtk_widget_get_parent(root_);
#endif
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

void FocusManager::RegisterAccelerator(
    const Accelerator& accelerator,
    AcceleratorTarget* target) {
  AcceleratorTargetList& targets = accelerators_[accelerator];
  DCHECK(std::find(targets.begin(), targets.end(), target) == targets.end())
      << "Registering the same target multiple times";
  targets.push_front(target);
}

void FocusManager::UnregisterAccelerator(const Accelerator& accelerator,
                                         AcceleratorTarget* target) {
  AcceleratorMap::iterator map_iter = accelerators_.find(accelerator);
  if (map_iter == accelerators_.end()) {
    NOTREACHED() << "Unregistering non-existing accelerator";
    return;
  }

  AcceleratorTargetList* targets = &map_iter->second;
  AcceleratorTargetList::iterator target_iter =
      std::find(targets->begin(), targets->end(), target);
  if (target_iter == targets->end()) {
    NOTREACHED() << "Unregistering accelerator for wrong target";
    return;
  }

  targets->erase(target_iter);
}

void FocusManager::UnregisterAccelerators(AcceleratorTarget* target) {
  for (AcceleratorMap::iterator map_iter = accelerators_.begin();
       map_iter != accelerators_.end(); ++map_iter) {
    AcceleratorTargetList* targets = &map_iter->second;
    targets->remove(target);
  }
}

bool FocusManager::ProcessAccelerator(const Accelerator& accelerator) {
  AcceleratorMap::iterator map_iter = accelerators_.find(accelerator);
  if (map_iter != accelerators_.end()) {
    // We have to copy the target list here, because an AcceleratorPressed
    // event handler may modify the list.
    AcceleratorTargetList targets(map_iter->second);
    for (AcceleratorTargetList::iterator iter = targets.begin();
         iter != targets.end(); ++iter) {
      if ((*iter)->AcceleratorPressed(accelerator))
        return true;
    }
  }

  // When dealing with child windows that have their own FocusManager (such
  // as ConstrainedWindow), we still want the parent FocusManager to process
  // the accelerator if the child window did not process it.
  FocusManager* parent_focus_manager = GetParentFocusManager();
  if (parent_focus_manager)
    return parent_focus_manager->ProcessAccelerator(accelerator);

  return false;
}

AcceleratorTarget* FocusManager::GetCurrentTargetForAccelerator(
    const views::Accelerator& accelerator) const {
  AcceleratorMap::const_iterator map_iter = accelerators_.find(accelerator);
  if (map_iter == accelerators_.end() || map_iter->second.empty())
    return NULL;
  return map_iter->second.front();
}

// static
bool FocusManager::IsTabTraversalKeyEvent(const KeyEvent& key_event) {
#if defined(OS_WIN)
  return key_event.GetCharacter() == VK_TAB && !win_util::IsCtrlPressed();
#else
  NOTIMPLEMENTED();
  return false;
#endif
}

void FocusManager::ViewRemoved(View* parent, View* removed) {
  if (focused_view_ && focused_view_ == removed)
    ClearFocus();
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
