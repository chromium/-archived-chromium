// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/views/window.h"

#include "base/win_util.h"
#include "chrome/app/chrome_dll_resource.h"
// TODO(beng): some day make this unfortunate dependency not exist.
#include "chrome/browser/browser_list.h"
#include "chrome/common/gfx/chrome_font.h"
#include "chrome/common/gfx/icon_util.h"
#include "chrome/common/l10n_util.h"
#include "chrome/common/pref_service.h"
#include "chrome/common/resource_bundle.h"
#include "chrome/common/win_util.h"
#include "chrome/views/client_view.h"
#include "chrome/views/custom_frame_window.h"
#include "chrome/views/non_client_view.h"
#include "chrome/views/root_view.h"
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
  if (IsMinimized())
    ::ShowWindow(GetHWND(), SW_RESTORE);
  ::SetWindowPos(GetHWND(), HWND_TOP, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE);
  SetForegroundWindow(GetHWND());
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

  if (client_view_->CanClose()) {
    SaveWindowPosition();
    RestoreEnabledIfNecessary();
    ContainerWin::Close();
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
                 SC_CLOSE, enable ? MF_ENABLED : MF_GRAYED);

  // Let the window know the frame changed.
  SetWindowPos(NULL, 0, 0, 0, 0,
               SWP_FRAMECHANGED | SWP_NOACTIVATE | SWP_NOCOPYBITS |
                   SWP_NOMOVE | SWP_NOOWNERZORDER | SWP_NOREPOSITION |
                   SWP_NOSENDCHANGING | SWP_NOSIZE | SWP_NOZORDER);
}

void Window::DisableInactiveRendering(bool disable) {
  disable_inactive_rendering_ = disable;
  if (!disable_inactive_rendering_)
    DefWindowProc(GetHWND(), WM_NCACTIVATE, FALSE, 0);
}

void Window::UpdateWindowTitle() {
  std::wstring window_title = window_delegate_->GetWindowTitle();
  std::wstring localized_text;
  if (l10n_util::AdjustStringForLocaleDirection(window_title, &localized_text))
    window_title.assign(localized_text);

  SetWindowText(GetHWND(), window_title.c_str());
}

void Window::UpdateWindowIcon() {
  SkBitmap icon = window_delegate_->GetWindowIcon();
  if (!icon.isNull()) {
    HICON windows_icon = IconUtil::CreateHICONFromSkBitmap(icon);
    // We need to make sure to destroy the previous icon, otherwise we'll leak
    // these GDI objects until we crash!
    HICON old_icon = reinterpret_cast<HICON>(
        SendMessage(GetHWND(), WM_SETICON, ICON_SMALL,
                    reinterpret_cast<LPARAM>(windows_icon)));
    if (old_icon)
      DestroyIcon(old_icon);
    old_icon = reinterpret_cast<HICON>(
        SendMessage(GetHWND(), WM_SETICON, ICON_BIG,
                    reinterpret_cast<LPARAM>(windows_icon)));
    if (old_icon)
      DestroyIcon(old_icon);
  }
}

void Window::ExecuteSystemMenuCommand(int command) {
  if (command)
    SendMessage(GetHWND(), WM_SYSCOMMAND, command, 0);
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

///////////////////////////////////////////////////////////////////////////////
// Window, protected:

Window::Window(WindowDelegate* window_delegate)
    : ContainerWin(),
      focus_on_creation_(true),
      window_delegate_(window_delegate),
      non_client_view_(NULL),
      client_view_(NULL),
      owning_hwnd_(NULL),
      minimum_size_(100, 100),
      is_modal_(false),
      restored_enabled_(false),
      is_always_on_top_(false),
      window_closed_(false),
      disable_inactive_rendering_(false) {
  InitClass();
  DCHECK(window_delegate_);
  window_delegate_->window_.reset(this);
  // Initialize these values to 0 so that subclasses can override the default
  // behavior before calling Init.
  set_window_style(0);
  set_window_ex_style(0);
  BrowserList::AddDependentWindow(this);
}

void Window::Init(HWND parent, const gfx::Rect& bounds) {
  // We need to save the parent window, since later calls to GetParent() will
  // return NULL.
  owning_hwnd_ = parent;
  // We call this after initializing our members since our implementations of
  // assorted ContainerWin functions may be called during initialization.
  is_modal_ = window_delegate_->IsModal();
  if (is_modal_)
    BecomeModal();
  is_always_on_top_ = window_delegate_->IsAlwaysOnTop();

  if (window_style() == 0)
    set_window_style(CalculateWindowStyle());
  if (window_ex_style() == 0)
    set_window_ex_style(CalculateWindowExStyle());

  ContainerWin::Init(parent, bounds, true);
  win_util::SetWindowUserData(GetHWND(), this);
  
  std::wstring window_title = window_delegate_->GetWindowTitle();
  std::wstring localized_text;
  if (l10n_util::AdjustStringForLocaleDirection(window_title, &localized_text))
    window_title.assign(localized_text);
  SetWindowText(GetHWND(), window_title.c_str());

  SetClientView(window_delegate_->CreateClientView(this));
  SetInitialBounds(bounds);

  if (window_delegate_->HasAlwaysOnTopMenu())
    AddAlwaysOnTopSystemMenuItem();
}

void Window::SetClientView(ClientView* client_view) {
  DCHECK(client_view && !client_view_ && GetHWND());
  client_view_ = client_view;
  if (non_client_view_) {
    // This will trigger the ClientView to be added by the non-client view.
    ContainerWin::SetContentsView(non_client_view_);
  } else {
    ContainerWin::SetContentsView(client_view_);
  }
}

void Window::SizeWindowToDefault() {
  gfx::Size pref;
  if (non_client_view_) {
    pref = non_client_view_->GetPreferredSize();
  } else {
    pref = client_view_->GetPreferredSize();
  }
  DCHECK(pref.width() > 0 && pref.height() > 0);
  // CenterAndSizeWindow adjusts the window size to accommodate the non-client
  // area.
  win_util::CenterAndSizeWindow(owning_window(), GetHWND(), pref.ToSIZE(),
                                true);
}

void Window::RunSystemMenu(const CPoint& point) {
  // We need to reset and clean up any currently created system menu objects.
  // We need to call this otherwise there's a small chance that we aren't going
  // to get a system menu. We also can't take the return value of this
  // function. We need to call it *again* to get a valid HMENU.
  ::GetSystemMenu(GetHWND(), TRUE);
  HMENU system_menu = ::GetSystemMenu(GetHWND(), FALSE);
  int id = ::TrackPopupMenu(system_menu,
                            TPM_LEFTBUTTON | TPM_RIGHTBUTTON | TPM_RETURNCMD,
                            point.x, point.y, 0, GetHWND(), NULL);
  ExecuteSystemMenuCommand(id);
}

///////////////////////////////////////////////////////////////////////////////
// Window, ContainerWin overrides:

void Window::OnActivate(UINT action, BOOL minimized, HWND window) {
  if (action == WA_INACTIVE)
    SaveWindowPosition();
}

LRESULT Window::OnAppCommand(HWND window, short app_command, WORD device,
                             int keystate) {
  // We treat APPCOMMAND ids as an extension of our command namespace, and just
  // let the delegate figure out what to do...
  if (!window_delegate_->ExecuteWindowsCommand(app_command))
    return ContainerWin::OnAppCommand(window, app_command, device, keystate);
  return 0;
}


void Window::OnCommand(UINT notification_code, int command_id, HWND window) {
  if (!window_delegate_->ExecuteWindowsCommand(command_id))
    ContainerWin::OnCommand(notification_code, command_id, window);
}

void Window::OnDestroy() {
  if (window_delegate_) {
    window_delegate_->WindowClosing();
    window_delegate_ = NULL;
  }
  RestoreEnabledIfNecessary();
  ContainerWin::OnDestroy();
}

LRESULT Window::OnEraseBkgnd(HDC dc) {
  SetMsgHandled(TRUE);
  return 1;
}

LRESULT Window::OnNCActivate(BOOL active) {
  if (disable_inactive_rendering_) {
    disable_inactive_rendering_ = false;
    return DefWindowProc(GetHWND(), WM_NCACTIVATE, TRUE, 0);
  }
  // Otherwise just do the default thing.
  return ContainerWin::OnNCActivate(active);
}

LRESULT Window::OnNCHitTest(const CPoint& point) {
  // First, give the ClientView a chance to test the point to see if it is part
  // of the non-client area.
  CPoint temp = point;
  MapWindowPoints(HWND_DESKTOP, GetHWND(), &temp, 1);
  int component = HTNOWHERE;
  if (non_client_view_) {
    component = non_client_view_->NonClientHitTest(gfx::Point(temp));
  } else {
    component = client_view_->NonClientHitTest(gfx::Point(temp));
  }
  if (component != HTNOWHERE)
    return component;

  // Otherwise, we let Windows do all the native frame non-client handling for
  // us.
  SetMsgHandled(FALSE);
  return 0;
}

void Window::OnNCLButtonDown(UINT ht_component, const CPoint& point) {
  if (non_client_view_ && ht_component == HTSYSMENU)
    RunSystemMenu(non_client_view_->GetSystemMenuPoint());
  ContainerWin::OnNCLButtonDown(ht_component, point);
}

void Window::OnNCRButtonDown(UINT ht_component, const CPoint& point) {
  if (ht_component == HTCAPTION || ht_component == HTSYSMENU) {
    RunSystemMenu(point);
  } else {
    ContainerWin::OnNCRButtonDown(ht_component, point);
  }
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
  if (root_view_->width() == new_size.cx &&
      root_view_->height() == new_size.cy)
    return;

  SaveWindowPosition();
  ChangeSize(size_param, new_size);
  RedrawWindow(GetHWND(), NULL, NULL, RDW_INVALIDATE | RDW_ALLCHILDREN);
}

void Window::OnSysCommand(UINT notification_code, CPoint click) {
  // First see if the delegate can handle it.
  if (window_delegate_->ExecuteWindowsCommand(notification_code))
    return;

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

void Window::SetInitialFocus() {
  if (!focus_on_creation_)
    return;

  ChromeViews::View* v = window_delegate_->GetInitiallyFocusedView();
  if (v) {
    v->RequestFocus();
  } else {
    // The window does not get keyboard messages unless we focus it, not sure
    // why.
    SetFocus(GetHWND());
  }
}

void Window::SetInitialBounds(const gfx::Rect& create_bounds) {
  // Restore the window's placement from the controller.
  CRect saved_bounds(create_bounds.ToRECT());
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
  } else {
    if (create_bounds.IsEmpty()) {
      // No initial bounds supplied, so size the window to its content and
      // center over its parent.
      SizeWindowToDefault();
    } else {
      // Use the supplied initial bounds.
      SetBounds(create_bounds);
    }
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
    //window_ex_styles |= WS_EX_APPWINDOW;
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

