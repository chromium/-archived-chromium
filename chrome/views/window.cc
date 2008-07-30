// Copyright 2008, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "chrome/views/window.h"

#include "chrome/app/chrome_dll_resource.h"
// TODO(beng): some day make this unfortunate dependency not exist.
#include "chrome/browser/browser_list.h"
#include "chrome/common/l10n_util.h"
#include "chrome/common/pref_service.h"
#include "chrome/common/resource_bundle.h"
#include "chrome/common/win_util.h"
#include "chrome/views/client_view.h"
#include "chrome/views/custom_frame_window.h"
#include "chrome/views/window_delegate.h"

#include "generated_resources.h"

namespace ChromeViews {

// static
HCURSOR Window::nwse_cursor_ = NULL;

// If the hung renderer warning doesn't fit on screen, the amount of padding to
// be left between the edge of the window and the edge of the nearest monitor,
// after the window is nudged back on screen. Pixels.
static const int kMonitorEdgePadding = 10;

////////////////////////////////////////////////////////////////////////////////
// Window, public:

Window::Window(WindowDelegate* window_delegate)
    : HWNDViewContainer(),
      focus_on_creation_(true),
      window_delegate_(window_delegate),
      client_view_(NULL),
      owning_hwnd_(NULL),
      minimum_size_(100, 100),
      is_modal_(false),
      restored_enabled_(false),
      is_always_on_top_(false),
      use_client_view_(true),
      accepted_(false),
      window_closed_(false) {
  InitClass();
  DCHECK(window_delegate_);
  window_delegate_->window_.reset(this);
  // Initialize these values to 0 so that subclasses can override the default
  // behavior before calling Init.
  set_window_style(0);
  set_window_ex_style(0);
  BrowserList::AddDependentWindow(this);
}

Window::~Window() {
  BrowserList::RemoveDependentWindow(this);
}

// static
Window* Window::CreateChromeWindow(HWND parent,
                                   const gfx::Rect& bounds,
                                   WindowDelegate* window_delegate) {
  Window* window = NULL;
  if (win_util::ShouldUseVistaFrame()) {
    window = new Window(window_delegate);
  } else {
    window = new CustomFrameWindow(window_delegate);
  }
  window->Init(parent, bounds);
  return window;
}

void Window::Init(HWND parent, const gfx::Rect& bounds) {
  // We need to save the parent window, since later calls to GetParent() will
  // return NULL.
  owning_hwnd_ = parent;
  // We call this after initializing our members since our implementations of
  // assorted HWNDViewContainer functions may be called during initialization.
  is_modal_ = window_delegate_->IsModal();
  if (is_modal_)
    BecomeModal();
  is_always_on_top_ = window_delegate_->IsAlwaysOnTop();

  if (window_style() == 0)
    set_window_style(CalculateWindowStyle());
  if (window_ex_style() == 0)
    set_window_ex_style(CalculateWindowExStyle());

  // A child window never owns its own focus manager, it uses the one
  // associated with the root of the window tree...
  if (use_client_view_) {
    View* contents_view = window_delegate_->GetContentsView();
    if (!contents_view)
      contents_view = new View;
    client_view_ = new ClientView(this, contents_view);
    // A Window almost always owns its own focus manager, even if it's a child
    // window. File a bug if you find a circumstance where this isn't the case
    // and we can adjust this API.  Note that if this is not the case, you'll
    // also have to change SetInitialFocus() as it relies on the window's focus
    // manager.
    HWNDViewContainer::Init(parent, bounds, client_view_, true);
  } else {
    HWNDViewContainer::Init(parent, bounds,
                            window_delegate_->GetContentsView(), true);
  }

  std::wstring window_title = window_delegate_->GetWindowTitle();
  SetWindowText(GetHWND(), window_title.c_str());

  win_util::SetWindowUserData(GetHWND(), this);

  // Restore the window's placement from the controller.
  CRect saved_bounds(0, 0, 0, 0);
  bool maximized = false;
  if (window_delegate_->RestoreWindowPosition(&saved_bounds,
                                              &maximized,
                                              &is_always_on_top_)) {
    // Make sure the bounds are at least the minimum size.
    if (saved_bounds.Width() < minimum_size_.cx) {
      saved_bounds.SetRect(saved_bounds.left, saved_bounds.top,
                           saved_bounds.right + minimum_size_.cx -
                              saved_bounds.Width(),
                           saved_bounds.bottom);
    }

    if (saved_bounds.Height() < minimum_size_.cy) {
      saved_bounds.SetRect(saved_bounds.left, saved_bounds.top,
                           saved_bounds.right,
                           saved_bounds.bottom + minimum_size_.cy -
                              saved_bounds.Height());
    }

    WINDOWPLACEMENT placement = {0};
    placement.length = sizeof(WINDOWPLACEMENT);
    placement.rcNormalPosition = saved_bounds;
    if (maximized)
      placement.showCmd = SW_SHOWMAXIMIZED;
    ::SetWindowPlacement(GetHWND(), &placement);

    if (is_always_on_top_ != window_delegate_->IsAlwaysOnTop())
      AlwaysOnTopChanged();
  } else if (bounds.IsEmpty()) {
    // Size the window to the content and center over the parent.
    SizeWindowToDefault();
  }

  if (window_delegate_->HasAlwaysOnTopMenu())
    AddAlwaysOnTopSystemMenuItem();
}

gfx::Size Window::CalculateWindowSizeForClientSize(
    const gfx::Size& client_size) const {
  RECT r = { 0, 0, client_size.width(), client_size.height() };
  ::AdjustWindowRectEx(&r, window_style(), FALSE, window_ex_style());
  return gfx::Size(r.right - r.left, r.bottom - r.top);
}

gfx::Size Window::CalculateMaximumSize() const {
  // If this is a top level window, the maximum size is the size of the working
  // rect of the display the window is on, less padding. If this is a child
  // (constrained) window, the maximum size of this Window are the bounds of the
  // parent window, less padding.
  DCHECK(GetHWND()) << "Cannot calculate maximum size before Init() is called";
  gfx::Rect working_rect;
  HWND parent_hwnd = ::GetParent(GetHWND());
  if (parent_hwnd) {
    RECT parent_rect;
    ::GetClientRect(parent_hwnd, &parent_rect);
    working_rect = parent_rect;
  } else {
    HMONITOR current_monitor =
        ::MonitorFromWindow(GetHWND(), MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi;
    mi.cbSize = sizeof(mi);
    ::GetMonitorInfo(current_monitor, &mi);
    working_rect = mi.rcWork;
  }
  working_rect.Inset(kMonitorEdgePadding, kMonitorEdgePadding);
  return working_rect.size();
}

void Window::Show() {
  ShowWindow(SW_SHOW);
  SetInitialFocus();
}

void Window::Activate() {
  ::SetWindowPos(GetHWND(), HWND_TOP, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE);
}

void Window::SetBounds(const gfx::Rect& bounds) {
  SetBounds(bounds, NULL);
}

void Window::SetBounds(const gfx::Rect& bounds, HWND other_hwnd) {
  win_util::SetChildBounds(GetHWND(), GetParent(), other_hwnd, bounds,
                           kMonitorEdgePadding, 0);
}

void Window::Close() {
  if (window_closed_) {
    // It appears we can hit this code path if you close a modal dialog then
    // close the last browser before the destructor is hit, which triggers
    // invoking Close again. I'm short circuiting this code path to avoid
    // calling into the delegate twice, which is problematic.
    return;
  }

  bool can_close = true;
  // Ask the delegate if we're allowed to close. The user may not have left the
  // window in a state where this is allowable (e.g. unsaved work). Also, don't
  // call Cancel on the delegate if we've already been accepted and are in the
  // process of being closed. Furthermore, if we have only an OK button, but no
  // Cancel button, and we're closing without being accepted, call Accept to
  // see if we should close.
  if (!accepted_) {
    DialogDelegate* dd = window_delegate_->AsDialogDelegate();
    if (dd) {
      int buttons = dd->GetDialogButtons();
      if (buttons & DialogDelegate::DIALOGBUTTON_CANCEL)
        can_close = dd->Cancel();
      else if (buttons & DialogDelegate::DIALOGBUTTON_OK)
        can_close = dd->Accept(true);
    }
  }
  if (can_close) {
    SaveWindowPosition();
    RestoreEnabledIfNecessary();
    HWNDViewContainer::Close();
    // If the user activates another app after opening us, then comes back and
    // closes us, we want our owner to gain activation.  But only if the owner
    // is visible. If we don't manually force that here, the other app will
    // regain activation instead.
    if (owning_hwnd_ && GetHWND() == GetForegroundWindow() &&
        IsWindowVisible(owning_hwnd_)) {
      SetForegroundWindow(owning_hwnd_);
    }
    window_closed_ = true;
  }
}

bool Window::IsMaximized() const {
  return !!::IsZoomed(GetHWND());
}

bool Window::IsMinimized() const {
  return !!::IsIconic(GetHWND());
}

void Window::EnableClose(bool enable) {
  EnableMenuItem(::GetSystemMenu(GetHWND(), false),
                 SC_CLOSE,
                 enable ? MF_ENABLED : MF_GRAYED);

  // Let the window know the frame changed.
  SetWindowPos(NULL, 0, 0, 0, 0,
               SWP_FRAMECHANGED |
               SWP_NOACTIVATE |
               SWP_NOCOPYBITS |
               SWP_NOMOVE |
               SWP_NOOWNERZORDER |
               SWP_NOREPOSITION |
               SWP_NOSENDCHANGING |
               SWP_NOSIZE |
               SWP_NOZORDER);
}

void Window::UpdateDialogButtons() {
  if (client_view_)
    client_view_->UpdateDialogButtons();
}

void Window::AcceptWindow() {
  accepted_ = true;
  DialogDelegate* dd = window_delegate_->AsDialogDelegate();
  if (dd)
    accepted_ = dd->Accept(false);
  if (accepted_)
    Close();
}

void Window::CancelWindow() {
  // Call the standard Close handler, which checks with the delegate before
  // proceeding. This checking _isn't_ done here, but in the WM_CLOSE handler,
  // so that the close box on the window also shares this code path.
  Close();
}

void Window::UpdateWindowTitle() {
  std::wstring window_title = window_delegate_->GetWindowTitle();
  SetWindowText(GetHWND(), window_title.c_str());
}

// static
bool Window::SaveWindowPositionToPrefService(PrefService* pref_service,
                                             const std::wstring& entry,
                                             const CRect& bounds,
                                             bool maximized,
                                             bool always_on_top) {
  DCHECK(pref_service);
  DictionaryValue* win_pref = pref_service->GetMutableDictionary(entry.c_str());
  DCHECK(win_pref);

  win_pref->SetInteger(L"left", bounds.left);
  win_pref->SetInteger(L"top", bounds.top);
  win_pref->SetInteger(L"right", bounds.right);
  win_pref->SetInteger(L"bottom", bounds.bottom);
  win_pref->SetBoolean(L"maximized", maximized);
  win_pref->SetBoolean(L"always_on_top", always_on_top);
  return true;
}

// static
bool Window::RestoreWindowPositionFromPrefService(PrefService* pref_service,
                                                  const std::wstring& entry,
                                                  CRect* bounds,
                                                  bool* maximized,
                                                  bool* always_on_top) {
  DCHECK(pref_service);
  DCHECK(bounds);
  DCHECK(maximized);
  DCHECK(always_on_top);

  const DictionaryValue* dictionary = pref_service->GetDictionary(entry.c_str());
  if (!dictionary)
    return false;

  int left, top, right, bottom;
  bool temp_maximized, temp_always_on_top;
  if (!dictionary || !dictionary->GetInteger(L"left", &left) ||
      !dictionary->GetInteger(L"top", &top) ||
      !dictionary->GetInteger(L"right", &right) ||
      !dictionary->GetInteger(L"bottom", &bottom) ||
      !dictionary->GetBoolean(L"maximized", &temp_maximized) ||
      !dictionary->GetBoolean(L"always_on_top", &temp_always_on_top))
    return false;

  bounds->SetRect(left, top, right, bottom);
  *maximized = temp_maximized;
  *always_on_top = temp_always_on_top;
  return true;
}

// static
gfx::Size Window::GetLocalizedContentsSize(int col_resource_id,
                                           int row_resource_id) {
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  ChromeFont font = rb.GetFont(ResourceBundle::BaseFont);

  double chars = _wtof(l10n_util::GetString(col_resource_id).c_str());
  double lines = _wtof(l10n_util::GetString(row_resource_id).c_str());

  int width = static_cast<int>(font.ave_char_width() * chars);
  int height = static_cast<int>(font.height() * lines);

  DCHECK(width > 0 && height > 0);

  return gfx::Size(width, height);
}

////////////////////////////////////////////////////////////////////////////////
// Window, protected:

void Window::SizeWindowToDefault() {
  if (client_view_) {
    CSize pref(0, 0);
    client_view_->GetPreferredSize(&pref);
    DCHECK(pref.cx > 0 && pref.cy > 0);
    // CenterAndSizeWindow adjusts the window size to accommodate the non-client
    // area.
    win_util::CenterAndSizeWindow(owning_window(), GetHWND(), pref, true);
  }
}

void Window::SetInitialFocus() {
  if (!focus_on_creation_)
    return;

  bool focus_set = false;
  ChromeViews::View* v = window_delegate_->GetInitiallyFocusedView();
  // For dialogs, try to focus either the OK or Cancel buttons if any.
  if (!v && window_delegate_->AsDialogDelegate() && client_view_) {
    if (client_view_->ok_button())
      v = client_view_->ok_button();
    else if (client_view_->cancel_button())
      v = client_view_->cancel_button();
  }
  if (v) {
    focus_set = true;
    // In order to make that view the initially focused one, we make it the
    // focused view on the focus manager and we store the focused view.
    // When the window is activated, the focus manager will restore the 
    // stored focused view.
    FocusManager* focus_manager = FocusManager::GetFocusManager(GetHWND());
    DCHECK(focus_manager);
    focus_manager->SetFocusedView(v);
    focus_manager->StoreFocusedView();
  }

  if (!focus_set && focus_on_creation_) {
    // The window does not get keyboard messages unless we focus it, not sure
    // why.
    SetFocus(GetHWND());
  }
}

////////////////////////////////////////////////////////////////////////////////
// Window, HWNDViewContainer overrides:

void Window::OnActivate(UINT action, BOOL minimized, HWND window) {
  if (action == WA_INACTIVE)
    SaveWindowPosition();
}

void Window::OnCommand(UINT notification_code, int command_id, HWND window) {
  window_delegate_->ExecuteWindowsCommand(command_id);
}

void Window::OnDestroy() {
  if (window_delegate_) {
    window_delegate_->WindowClosing();
    window_delegate_ = NULL;
  }
  RestoreEnabledIfNecessary();
  HWNDViewContainer::OnDestroy();
}

LRESULT Window::OnEraseBkgnd(HDC dc) {
  SetMsgHandled(TRUE);
  return 1;
}

LRESULT Window::OnNCHitTest(const CPoint& point) {
  // We paint the size box over the content area sometimes... check to see if
  // the mouse is over it...
  CPoint temp = point;
  MapWindowPoints(HWND_DESKTOP, GetHWND(), &temp, 1);
  if (client_view_ && client_view_->PointIsInSizeBox(gfx::Point(temp)))
    return HTBOTTOMRIGHT;

  // Otherwise, we let Windows do all the native frame non-client handling for
  // us.
  SetMsgHandled(FALSE);
  return 0;
}

LRESULT Window::OnSetCursor(HWND window, UINT hittest_code, UINT message) {
  if (hittest_code == HTBOTTOMRIGHT) {
    // If the mouse was over the resize gripper, make sure the right cursor is
    // supplied...
    SetCursor(nwse_cursor_);
    return TRUE;
  }
  // Otherwise just let Windows do the rest.
  SetMsgHandled(FALSE);
  return TRUE;
}

void Window::OnSize(UINT size_param, const CSize& new_size) {
  if (root_view_->GetWidth() == new_size.cx &&
      root_view_->GetHeight() == new_size.cy)
    return;

  SaveWindowPosition();
  ChangeSize(size_param, new_size);
  RedrawWindow(GetHWND(), NULL, NULL, RDW_INVALIDATE | RDW_ALLCHILDREN);
}

void Window::OnSysCommand(UINT notification_code, CPoint click) {
  if (notification_code == IDC_ALWAYS_ON_TOP) {
    is_always_on_top_ = !is_always_on_top_;

    // Change the menu check state.
    HMENU system_menu = ::GetSystemMenu(GetHWND(), FALSE);
    MENUITEMINFO menu_info;
    memset(&menu_info, 0, sizeof(MENUITEMINFO));
    menu_info.cbSize = sizeof(MENUITEMINFO);
    BOOL r = ::GetMenuItemInfo(system_menu, IDC_ALWAYS_ON_TOP,
                               FALSE, &menu_info);
    DCHECK(r);
    menu_info.fMask = MIIM_STATE;
    if (is_always_on_top_)
      menu_info.fState = MFS_CHECKED;
    r = ::SetMenuItemInfo(system_menu, IDC_ALWAYS_ON_TOP, FALSE, &menu_info);

    // Now change the actual window's behavior.
    AlwaysOnTopChanged();
  } else {
    // Use the default implementation for any other command.
    DefWindowProc(GetHWND(), WM_SYSCOMMAND, notification_code,
                  MAKELPARAM(click.y, click.x));
  }
}

////////////////////////////////////////////////////////////////////////////////
// Window, private:

void Window::BecomeModal() {
  // We implement modality by crawling up the hierarchy of windows starting
  // at the owner, disabling all of them so that they don't receive input
  // messages.
  DCHECK(owning_hwnd_) << "Can't create a modal dialog without an owner";
  HWND start = owning_hwnd_;
  while (start != NULL) {
    ::EnableWindow(start, FALSE);
    start = ::GetParent(start);
  }
}

void Window::AddAlwaysOnTopSystemMenuItem() {
  // The Win32 API requires that we own the text.
  always_on_top_menu_text_ = l10n_util::GetString(IDS_ALWAYS_ON_TOP);

  // Let's insert a menu to the window.
  HMENU system_menu = ::GetSystemMenu(GetHWND(), FALSE);
  int index = ::GetMenuItemCount(system_menu) - 1;
  if (index < 0) {
    // Paranoia check.
    NOTREACHED();
    index = 0;
  }
  // First we add the separator.
  MENUITEMINFO menu_info;
  memset(&menu_info, 0, sizeof(MENUITEMINFO));
  menu_info.cbSize = sizeof(MENUITEMINFO);
  menu_info.fMask = MIIM_FTYPE;
  menu_info.fType = MFT_SEPARATOR;
  ::InsertMenuItem(system_menu, index, TRUE, &menu_info);

  // Then the actual menu.
  menu_info.fMask = MIIM_FTYPE | MIIM_ID | MIIM_STRING | MIIM_STATE;
  menu_info.fType = MFT_STRING;
  menu_info.fState = MFS_ENABLED;
  if (is_always_on_top_)
    menu_info.fState |= MFS_CHECKED;
  menu_info.wID = IDC_ALWAYS_ON_TOP;
  menu_info.dwTypeData = const_cast<wchar_t*>(always_on_top_menu_text_.c_str());
  ::InsertMenuItem(system_menu, index, TRUE, &menu_info);
}

void Window::RestoreEnabledIfNecessary() {
  if (is_modal_ && !restored_enabled_) {
    restored_enabled_ = true;
    // If we were run modally, we need to undo the disabled-ness we inflicted on
    // the owner's parent hierarchy.
    HWND start = owning_hwnd_;
    while (start != NULL) {
      ::EnableWindow(start, TRUE);
      start = ::GetParent(start);
    }
  }
}

void Window::AlwaysOnTopChanged() {
  ::SetWindowPos(GetHWND(),
    is_always_on_top_ ? HWND_TOPMOST : HWND_NOTOPMOST,
    0, 0, 0, 0,
    SWP_NOMOVE | SWP_NOSIZE | SWP_FRAMECHANGED);
}

DWORD Window::CalculateWindowStyle() {
  DWORD window_styles = WS_CLIPCHILDREN | WS_CLIPSIBLINGS | WS_SYSMENU;
  bool can_resize = window_delegate_->CanResize();
  bool can_maximize = window_delegate_->CanMaximize();
  if ((can_resize && can_maximize) || can_maximize) {
    window_styles |= WS_OVERLAPPEDWINDOW;
  } else if (can_resize) {
    window_styles |= WS_OVERLAPPED | WS_THICKFRAME;
  }
  if (window_delegate_->AsDialogDelegate()) {
    window_styles |= DS_MODALFRAME;
    // NOTE: Turning this off means we lose the close button, which is bad.
    // Turning it on though means the user can maximize or size the window
    // from the system menu, which is worse. We may need to provide our own
    // menu to get the close button to appear properly.
    // window_styles &= ~WS_SYSMENU;
  }
  return window_styles;
}

DWORD Window::CalculateWindowExStyle() {
  DWORD window_ex_styles = 0;
  if (window_delegate_->AsDialogDelegate()) {
    window_ex_styles |= WS_EX_DLGMODALFRAME;
  } else if (!(window_style() & WS_CHILD)) {
    window_ex_styles |= WS_EX_APPWINDOW;
  }
  if (window_delegate_->IsAlwaysOnTop())
    window_ex_styles |= WS_EX_TOPMOST;
  return window_ex_styles;
}

void Window::SaveWindowPosition() {
  WINDOWPLACEMENT win_placement = { 0 };
  win_placement.length = sizeof(WINDOWPLACEMENT);

  BOOL r = GetWindowPlacement(GetHWND(), &win_placement);
  DCHECK(r);

  bool maximized = (win_placement.showCmd == SW_SHOWMAXIMIZED);
  CRect window_bounds(win_placement.rcNormalPosition);
  window_delegate_->SaveWindowPosition(window_bounds,
                                       maximized,
                                       is_always_on_top_);
}

void Window::InitClass() {
  static bool initialized = false;
  if (!initialized) {
    nwse_cursor_ = LoadCursor(NULL, IDC_SIZENWSE);
    initialized = true;
  }
}

}  // namespace ChromeViews
