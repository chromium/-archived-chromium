// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/views/window.h"

#include "base/win_util.h"
#include "chrome/app/chrome_dll_resource.h"
#include "chrome/common/gfx/chrome_font.h"
#include "chrome/common/gfx/icon_util.h"
#include "chrome/common/gfx/path.h"
#include "chrome/common/l10n_util.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/pref_service.h"
#include "chrome/common/resource_bundle.h"
#include "chrome/common/win_util.h"
#include "chrome/views/client_view.h"
#include "chrome/views/custom_frame_window.h"
#include "chrome/views/default_non_client_view.h"
#include "chrome/views/non_client_view.h"
#include "chrome/views/root_view.h"
#include "chrome/views/window_delegate.h"
#include "grit/generated_resources.h"

namespace views {

// A scoping class that prevents a window from being able to redraw in response
// to invalidations that may occur within it for the lifetime of the object.
//
// Why would we want such a thing? Well, it turns out Windows has some
// "unorthodox" behavior when it comes to painting its non-client areas.
// Occasionally, Windows will paint portions of the default non-client area
// right over the top of the custom frame. This is not simply fixed by handling
// WM_NCPAINT/WM_PAINT, with some investigation it turns out that this
// rendering is being done *inside* the default implementation of some message
// handlers and functions:
//  . WM_SETTEXT
//  . WM_SETICON
//  . WM_NCLBUTTONDOWN
//  . EnableMenuItem, called from our WM_INITMENU handler
// The solution is to handle these messages and call DefWindowProc ourselves,
// but prevent the window from being able to update itself for the duration of
// the call. We do this with this class, which automatically calls its
// associated Window's lock and unlock functions as it is created and destroyed.
// See documentation in those methods for the technique used.
//
// IMPORTANT: Do not use this scoping object for large scopes or periods of
//            time! IT WILL PREVENT THE WINDOW FROM BEING REDRAWN! (duh).
//
// I would love to hear Raymond Chen's explanation for all this. And maybe a
// list of other messages that this applies to ;-)
class Window::ScopedRedrawLock {
 public:
  explicit ScopedRedrawLock(Window* window) : window_(window) {
    window_->LockUpdates();
  }

  ~ScopedRedrawLock() {
    window_->UnlockUpdates();
  }

 private:
  // The window having its style changed.
  Window* window_;
};

HCURSOR Window::resize_cursors_[6];

// If the hung renderer warning doesn't fit on screen, the amount of padding to
// be left between the edge of the window and the edge of the nearest monitor,
// after the window is nudged back on screen. Pixels.
static const int kMonitorEdgePadding = 10;

////////////////////////////////////////////////////////////////////////////////
// Window, public:

Window::~Window() {
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
  int show_state = GetShowState();
  if (saved_maximized_state_)
    show_state = SW_SHOWMAXIMIZED;
  Show(show_state);
}

void Window::Show(int show_state) {
  ShowWindow(show_state);
  // When launched from certain programs like bash and Windows Live Messenger,
  // show_state is set to SW_HIDE, so we need to correct that condition. We
  // don't just change show_state to SW_SHOWNORMAL because MSDN says we must
  // always first call ShowWindow with the specified value from STARTUPINFO,
  // otherwise all future ShowWindow calls will be ignored (!!#@@#!). Instead,
  // we call ShowWindow again in this case.
  if (show_state == SW_HIDE)
    ShowWindow(SW_SHOWNORMAL);
  SetInitialFocus();
}

int Window::GetShowState() const {
  return SW_SHOWNORMAL;
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

  if (non_client_view_->CanClose()) {
    SaveWindowPosition();
    RestoreEnabledIfNecessary();
    WidgetWin::Close();
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
  // If the native frame is rendering its own close button, ask it to disable.
  non_client_view_->EnableClose(enable);

  // Disable the native frame's close button regardless of whether or not the
  // native frame is in use, since this also affects the system menu.
  EnableMenuItem(GetSystemMenu(GetHWND(), false),
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
  
  if (!non_client_view_->UseNativeFrame()) {
    // If the non-client view is rendering its own frame, we need to forcibly
    // schedule a paint so it updates when we unset this mode.
    non_client_view_->set_paint_as_active(disable);
    if (!disable)
      non_client_view_->SchedulePaint();
  }
}

void Window::UpdateWindowTitle() {
  // If the non-client view is rendering its own title, it'll need to relayout
  // now.
  non_client_view_->Layout();

  // Update the native frame's text. We do this regardless of whether or not
  // the native frame is being used, since this also updates the taskbar, etc.
  std::wstring window_title = window_delegate_->GetWindowTitle();
  std::wstring localized_text;
  if (l10n_util::AdjustStringForLocaleDirection(window_title, &localized_text))
    window_title.assign(localized_text);
  SetWindowText(GetHWND(), window_title.c_str());
}

void Window::UpdateWindowIcon() {
  // If the non-client view is rendering its own icon, we need to tell it to repaint.
  non_client_view_->SchedulePaint();

  // Update the native frame's icon. We do this regardless of whether or not
  // the native frame is being used, since this also updates the taskbar, etc.
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
int Window::GetLocalizedContentsWidth(int col_resource_id) {
  double chars = _wtof(l10n_util::GetString(col_resource_id).c_str());
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  ChromeFont font = rb.GetFont(ResourceBundle::BaseFont);
  int width = font.GetExpectedTextWidth(static_cast<int>(chars));
  DCHECK(width > 0);
  return width;
}

// static
int Window::GetLocalizedContentsHeight(int row_resource_id) {
  double lines = _wtof(l10n_util::GetString(row_resource_id).c_str());
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  ChromeFont font = rb.GetFont(ResourceBundle::BaseFont);
  int height = static_cast<int>(font.height() * lines);
  DCHECK(height > 0);
  return height;
}

// static
gfx::Size Window::GetLocalizedContentsSize(int col_resource_id,
                                           int row_resource_id) {
  return gfx::Size(GetLocalizedContentsWidth(col_resource_id),
                   GetLocalizedContentsHeight(row_resource_id));
}

////////////////////////////////////////////////////////////////////////////////
// Window, NotificationObserver implementation:

void Window::Observe(NotificationType type,
                     const NotificationSource& source,
                     const NotificationDetails& details) {
  // This window is closed when the last app window is closed.
  DCHECK(type == NotificationType::ALL_APPWINDOWS_CLOSED);
  // Only registered as an observer when we're not an app window.
  // XXX DCHECK(!IsAppWindow());
  Close();
}

///////////////////////////////////////////////////////////////////////////////
// Window, protected:

Window::Window(WindowDelegate* window_delegate)
    : WidgetWin(),
      focus_on_creation_(true),
      window_delegate_(window_delegate),
      non_client_view_(new NonClientView),
      owning_hwnd_(NULL),
      minimum_size_(100, 100),
      is_modal_(false),
      restored_enabled_(false),
      is_always_on_top_(false),
      window_closed_(false),
      disable_inactive_rendering_(false),
      is_active_(false),
      lock_updates_(false),
      saved_window_style_(0),
      saved_maximized_state_(0) {
  InitClass();
  DCHECK(window_delegate_);
  window_delegate_->window_.reset(this);
  // Initialize these values to 0 so that subclasses can override the default
  // behavior before calling Init.
  set_window_style(0);
  set_window_ex_style(0);
}

void Window::Init(HWND parent, const gfx::Rect& bounds) {
  // We need to save the parent window, since later calls to GetParent() will
  // return NULL.
  owning_hwnd_ = parent;
  // We call this after initializing our members since our implementations of
  // assorted WidgetWin functions may be called during initialization.
  is_modal_ = window_delegate_->IsModal();
  if (is_modal_)
    BecomeModal();
  is_always_on_top_ = window_delegate_->IsAlwaysOnTop();

  if (window_style() == 0)
    set_window_style(CalculateWindowStyle());
  if (window_ex_style() == 0)
    set_window_ex_style(CalculateWindowExStyle());

  WidgetWin::Init(parent, bounds, true);
  win_util::SetWindowUserData(GetHWND(), this);
  
  std::wstring window_title = window_delegate_->GetWindowTitle();
  std::wstring localized_text;
  if (l10n_util::AdjustStringForLocaleDirection(window_title, &localized_text))
    window_title.assign(localized_text);
  SetWindowText(GetHWND(), window_title.c_str());

  SetClientView(window_delegate_->CreateClientView(this));
  SetInitialBounds(bounds);
  InitAlwaysOnTopState();

  if (!IsAppWindow()) {
    notification_registrar_.Add(
        this,
        NotificationType::ALL_APPWINDOWS_CLOSED,
        NotificationService::AllSources());
  }

  ResetWindowRegion();
}

void Window::SizeWindowToDefault() {
  // CenterAndSizeWindow adjusts the window size to accommodate the non-client
  // area if we're using a native frame.
  win_util::CenterAndSizeWindow(owning_window(), GetHWND(),
                                non_client_view_->GetPreferredSize().ToSIZE(),
                                non_client_view_->UseNativeFrame());
}

void Window::RunSystemMenu(const gfx::Point& point) {
  // We need to reset and clean up any currently created system menu objects.
  // We need to call this otherwise there's a small chance that we aren't going
  // to get a system menu. We also can't take the return value of this
  // function. We need to call it *again* to get a valid HMENU.
  //::GetSystemMenu(GetHWND(), TRUE);
  HMENU system_menu = ::GetSystemMenu(GetHWND(), FALSE);
  int id = ::TrackPopupMenu(system_menu,
                            TPM_LEFTBUTTON | TPM_RIGHTBUTTON | TPM_RETURNCMD,
                            point.x(), point.y(), 0, GetHWND(), NULL);
  ExecuteSystemMenuCommand(id);
}

///////////////////////////////////////////////////////////////////////////////
// Window, WidgetWin overrides:

void Window::OnActivate(UINT action, BOOL minimized, HWND window) {
  if (action == WA_INACTIVE)
    SaveWindowPosition();
}

LRESULT Window::OnAppCommand(HWND window, short app_command, WORD device,
                             int keystate) {
  // We treat APPCOMMAND ids as an extension of our command namespace, and just
  // let the delegate figure out what to do...
  if (!window_delegate_->ExecuteWindowsCommand(app_command))
    return WidgetWin::OnAppCommand(window, app_command, device, keystate);
  return 0;
}

void Window::OnCommand(UINT notification_code, int command_id, HWND window) {
  // We NULL check |window_delegate_| here because we can be sent WM_COMMAND
  // messages even after the window is destroyed.
  // If the notification code is > 1 it means it is control specific and we
  // should ignore it.
  if (notification_code > 1 || !window_delegate_ ||
      window_delegate_->ExecuteWindowsCommand(command_id)) {
    WidgetWin::OnCommand(notification_code, command_id, window);
  }
}

void Window::OnDestroy() {
  non_client_view_->WindowClosing();
  window_delegate_ = NULL;
  RestoreEnabledIfNecessary();
  WidgetWin::OnDestroy();
}

namespace {
static void EnableMenuItem(HMENU menu, UINT command, bool enabled) {
  UINT flags = MF_BYCOMMAND | (enabled ? MF_ENABLED : MF_DISABLED | MF_GRAYED);
  EnableMenuItem(menu, command, flags);
}
}  // namespace

void Window::OnInitMenu(HMENU menu) {
  // We only need to manually enable the system menu if we're not using a native
  // frame.
  if (non_client_view_->UseNativeFrame()) {
    SetMsgHandled(FALSE);
    return;
  }

  bool is_minimized = IsMinimized();
  bool is_maximized = IsMaximized();
  bool is_restored = !is_minimized && !is_maximized;

  ScopedRedrawLock lock(this);
  EnableMenuItem(menu, SC_RESTORE, !is_restored);
  EnableMenuItem(menu, SC_MOVE, is_restored);
  EnableMenuItem(menu, SC_SIZE, window_delegate()->CanResize() && is_restored);
  EnableMenuItem(menu, SC_MAXIMIZE,
                 window_delegate()->CanMaximize() && !is_maximized);
  EnableMenuItem(menu, SC_MINIMIZE,
                 window_delegate()->CanMaximize() && !is_minimized);
}

void Window::OnMouseLeave() {
  // We only need to manually track WM_MOUSELEAVE messages between the client
  // and non-client area when we're not using the native frame.
  if (non_client_view_->UseNativeFrame()) {
    SetMsgHandled(FALSE);
    return;
  }

  bool process_mouse_exited = true;
  POINT pt;
  if (GetCursorPos(&pt)) {
    LRESULT ht_component =
        ::SendMessage(GetHWND(), WM_NCHITTEST, 0, MAKELPARAM(pt.x, pt.y));
    if (ht_component != HTNOWHERE) {
      // If the mouse moved into a part of the window's non-client area, then
      // don't send a mouse exited event since the mouse is still within the
      // bounds of the ChromeView that's rendering the frame. Note that we do
      // _NOT_ do this for windows with native frames, since in that case the
      // mouse really will have left the bounds of the RootView.
      process_mouse_exited = false;
    }
  }

  if (process_mouse_exited)
    ProcessMouseExited();
}

LRESULT Window::OnNCActivate(BOOL active) {
  // If we're not using the native frame, we need to force a synchronous repaint
  // otherwise we'll be left in the wrong activation state until something else
  // causes a repaint later.
  if (!non_client_view_->UseNativeFrame()) {
    is_active_ = !!active;

    // We can get WM_NCACTIVATE before we're actually visible. If we're not
    // visible, no need to paint.
    if (IsWindowVisible(GetHWND())) {
      non_client_view_->SchedulePaint();
      // We need to force a paint now, as a user dragging a window will block
      // painting operations while the move is in progress.
      PaintNow(root_view_->GetScheduledPaintRect());
    }
  }

  if (disable_inactive_rendering_) {
    disable_inactive_rendering_ = false;
    return DefWindowProc(GetHWND(), WM_NCACTIVATE, TRUE, 0);
  }
  // Otherwise just do the default thing.
  return WidgetWin::OnNCActivate(active);
}

LRESULT Window::OnNCCalcSize(BOOL mode, LPARAM l_param) {
  // We only need to adjust the client size/paint handling when we're not using
  // the native frame.
  if (non_client_view_->UseNativeFrame()) {
    SetMsgHandled(FALSE);
    return 0;
  }

  // We need to repaint all when the window bounds change.
  return WVR_REDRAW;
}

LRESULT Window::OnNCHitTest(const CPoint& point) {
  // First, give the NonClientView a chance to test the point to see if it
  // provides any of the non-client area.
  CPoint temp = point;
  MapWindowPoints(HWND_DESKTOP, GetHWND(), &temp, 1);
  int component = non_client_view_->NonClientHitTest(gfx::Point(temp));
  if (component != HTNOWHERE)
    return component;

  // Otherwise, we let Windows do all the native frame non-client handling for
  // us.
  SetMsgHandled(FALSE);
  return 0;
}

namespace {
struct ClipState {
  // The window being painted.
  HWND parent;

  // DC painting to.
  HDC dc;

  // Origin of the window in terms of the screen.
  int x;
  int y;
};

// See comments in OnNCPaint for details of this function.
static BOOL CALLBACK ClipDCToChild(HWND window, LPARAM param) {
  ClipState* clip_state = reinterpret_cast<ClipState*>(param);
  if (GetParent(window) == clip_state->parent && IsWindowVisible(window)) {
    RECT bounds;
    GetWindowRect(window, &bounds);
    ExcludeClipRect(clip_state->dc,
                    bounds.left - clip_state->x,
                    bounds.top - clip_state->y,
                    bounds.right - clip_state->x,
                    bounds.bottom - clip_state->y);
  }
  return TRUE;
}
}  // namespace

void Window::OnNCPaint(HRGN rgn) {
  // We only do non-client painting if we're not using the native frame.
  if (non_client_view_->UseNativeFrame()) {
    SetMsgHandled(FALSE);
    return;
  }

  // We have an NC region and need to paint it. We expand the NC region to
  // include the dirty region of the root view. This is done to minimize
  // paints.
  CRect window_rect;
  GetWindowRect(&window_rect);

  if (window_rect.Width() != root_view_->width() ||
      window_rect.Height() != root_view_->height()) {
    // If the size of the window differs from the size of the root view it
    // means we're being asked to paint before we've gotten a WM_SIZE. This can
    // happen when the user is interactively resizing the window. To avoid
    // mass flickering we don't do anything here. Once we get the WM_SIZE we'll
    // reset the region of the window which triggers another WM_NCPAINT and
    // all is well.
    return;
  }

  CRect dirty_region;
  // A value of 1 indicates paint all.
  if (!rgn || rgn == reinterpret_cast<HRGN>(1)) {
    dirty_region = CRect(0, 0, window_rect.Width(), window_rect.Height());
  } else {
    RECT rgn_bounding_box;
    GetRgnBox(rgn, &rgn_bounding_box);
    if (!IntersectRect(&dirty_region, &rgn_bounding_box, &window_rect))
      return;  // Dirty region doesn't intersect window bounds, bale.

    // rgn_bounding_box is in screen coordinates. Map it to window coordinates.
    OffsetRect(&dirty_region, -window_rect.left, -window_rect.top);
  }

  // In theory GetDCEx should do what we want, but I couldn't get it to work.
  // In particular the docs mentiond DCX_CLIPCHILDREN, but as far as I can tell
  // it doesn't work at all. So, instead we get the DC for the window then
  // manually clip out the children.
  HDC dc = GetWindowDC(GetHWND());
  ClipState clip_state;
  clip_state.x = window_rect.left;
  clip_state.y = window_rect.top;
  clip_state.parent = GetHWND();
  clip_state.dc = dc;
  EnumChildWindows(GetHWND(), &ClipDCToChild,
                   reinterpret_cast<LPARAM>(&clip_state));

  RootView* root_view = GetRootView();
  CRect old_paint_region = root_view->GetScheduledPaintRectConstrainedToSize();

  if (!old_paint_region.IsRectEmpty()) {
    // The root view has a region that needs to be painted. Include it in the
    // region we're going to paint.

    CRect tmp = dirty_region;
    UnionRect(&dirty_region, &tmp, &old_paint_region);
  }

  root_view->SchedulePaint(gfx::Rect(dirty_region), false);

  // ChromeCanvasPaints destructor does the actual painting. As such, wrap the
  // following in a block to force paint to occur so that we can release the dc.
  {
    ChromeCanvasPaint canvas(dc, opaque(), dirty_region.left, dirty_region.top,
                             dirty_region.Width(), dirty_region.Height());

    root_view->ProcessPaint(&canvas);
  }

  ReleaseDC(GetHWND(), dc);
}

void Window::OnNCLButtonDown(UINT ht_component, const CPoint& point) {
  // When we're using a native frame, window controls work without us
  // interfering.
  if (!non_client_view_->UseNativeFrame()) {
    switch (ht_component) {
      case HTCLOSE:
      case HTMINBUTTON:
      case HTMAXBUTTON: {
        // When the mouse is pressed down in these specific non-client areas, we
        // need to tell the RootView to send the mouse pressed event (which sets
        // capture, allowing subsequent WM_LBUTTONUP (note, _not_ WM_NCLBUTTONUP)
        // to fire so that the appropriate WM_SYSCOMMAND can be sent by the
        // applicable button's ButtonListener. We _have_ to do this this way
        // rather than letting Windows just send the syscommand itself (as would
        // happen if we never did this dance) because for some insane reason
        // DefWindowProc for WM_NCLBUTTONDOWN also renders the pressed window
        // control button appearance, in the Windows classic style, over our
        // view! Ick! By handling this message we prevent Windows from doing this
        // undesirable thing, but that means we need to roll the sys-command
        // handling ourselves.
        ProcessNCMousePress(point, MK_LBUTTON);
        return;
      }
    }
  }

  // TODO(beng): figure out why we need to run the system menu manually
  //             ourselves. This is wrong and causes many subtle bugs.
  //             From my initial research, it looks like DefWindowProc tries
  //             to run it but fails before sending the initial WM_MENUSELECT
  //             for the sysmenu.
  if (ht_component == HTSYSMENU)
    RunSystemMenu(non_client_view_->GetSystemMenuPoint());
  else
    WidgetWin::OnNCLButtonDown(ht_component, point);

  /* TODO(beng): Fix the standard non-client over-painting bug. This code
                 doesn't work but identifies the problem.
  if (!IsMsgHandled()) {
    // Window::OnNCLButtonDown set the message as unhandled. This normally
    // means WidgetWin::ProcessWindowMessage will pass it to
    // DefWindowProc. Sadly, DefWindowProc for WM_NCLBUTTONDOWN does weird
    // non-client painting, so we need to call it directly here inside a
    // scoped update lock.
    ScopedRedrawLock lock(this);
    DefWindowProc(GetHWND(), WM_NCLBUTTONDOWN, ht_component,
                  MAKELPARAM(point.x, point.y));
    SetMsgHandled(TRUE);
  }
  */
}

void Window::OnNCRButtonDown(UINT ht_component, const CPoint& point) {
  if (ht_component == HTCAPTION || ht_component == HTSYSMENU)
    RunSystemMenu(gfx::Point(point));
  else
    WidgetWin::OnNCRButtonDown(ht_component, point);
}

LRESULT Window::OnNCUAHDrawCaption(UINT msg, WPARAM w_param,
                                              LPARAM l_param) {
  // See comment in widget_win.h at the definition of WM_NCUAHDRAWCAPTION for
  // an explanation about why we need to handle this message.
  SetMsgHandled(!non_client_view_->UseNativeFrame());
  return 0;
}

LRESULT Window::OnNCUAHDrawFrame(UINT msg, WPARAM w_param,
                                            LPARAM l_param) {
  // See comment in widget_win.h at the definition of WM_NCUAHDRAWCAPTION for
  // an explanation about why we need to handle this message.
  SetMsgHandled(!non_client_view_->UseNativeFrame());
  return 0;
}

LRESULT Window::OnSetCursor(HWND window, UINT hittest_code, UINT message) {
  int index = RC_NORMAL;
  switch (hittest_code) {
    case HTTOP:
    case HTBOTTOM:
      index = RC_VERTICAL;
      break;
    case HTTOPLEFT:
    case HTBOTTOMRIGHT:
      index = RC_NWSE;
      break;
    case HTTOPRIGHT:
    case HTBOTTOMLEFT:
      index = RC_NESW;
      break;
    case HTLEFT:
    case HTRIGHT:
      index = RC_HORIZONTAL;
      break;
    case HTCAPTION:
    case HTCLIENT:
      index = RC_NORMAL;
      break;
  }
  SetCursor(resize_cursors_[index]);
  return 0;
}

LRESULT Window::OnSetIcon(UINT size_type, HICON new_icon) {
  // This shouldn't hurt even if we're using the native frame.
  ScopedRedrawLock lock(this);
  return DefWindowProc(GetHWND(), WM_SETICON, size_type,
                       reinterpret_cast<LPARAM>(new_icon));
}

LRESULT Window::OnSetText(const wchar_t* text) {
  // This shouldn't hurt even if we're using the native frame.
  ScopedRedrawLock lock(this);
  return DefWindowProc(GetHWND(), WM_SETTEXT, NULL,
                       reinterpret_cast<LPARAM>(text));
}

void Window::OnSize(UINT size_param, const CSize& new_size) {
  // Don't no-op if the new_size matches current size. If our normal bounds
  // and maximized bounds are the same, then we need to layout (because we
  // layout differently when maximized).
  SaveWindowPosition();
  ChangeSize(size_param, new_size);
  RedrawWindow(GetHWND(), NULL, NULL, RDW_INVALIDATE | RDW_ALLCHILDREN);

  // ResetWindowRegion is going to trigger WM_NCPAINT. By doing it after we've
  // invoked OnSize we ensure the RootView has been laid out.
  ResetWindowRegion();
}

void Window::OnSysCommand(UINT notification_code, CPoint click) {
  if (!non_client_view_->UseNativeFrame()) {
    // Windows uses the 4 lower order bits of |notification_code| for type-
    // specific information so we must exclude this when comparing.
    static const int sc_mask = 0xFFF0;
    if ((notification_code & sc_mask) == SC_MINIMIZE ||
        (notification_code & sc_mask) == SC_MAXIMIZE ||
        (notification_code & sc_mask) == SC_RESTORE) {
      non_client_view_->ResetWindowControls();
    } else if ((notification_code & sc_mask) == SC_MOVE ||
               (notification_code & sc_mask) == SC_SIZE) {
      if (lock_updates_) {
        // We were locked, before entering a resize or move modal loop. Now that
        // we've begun to move the window, we need to unlock updates so that the
        // sizing/moving feedback can be continuous.
        UnlockUpdates();
      }
    }
  }

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
  } else if ((notification_code == SC_KEYMENU) && (click.x == VK_SPACE)) {
    // Run the system menu at the NonClientView's desired location.
    RunSystemMenu(non_client_view_->GetSystemMenuPoint());
  } else {
    // Use the default implementation for any other command.
    DefWindowProc(GetHWND(), WM_SYSCOMMAND, notification_code,
                  MAKELPARAM(click.y, click.x));
  }
}

////////////////////////////////////////////////////////////////////////////////
// Window, private:

void Window::SetClientView(ClientView* client_view) {
  DCHECK(client_view && GetHWND());
  non_client_view_->set_client_view(client_view);
  // This will trigger the ClientView to be added by the non-client view.
  WidgetWin::SetContentsView(non_client_view_);
}

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

  View* v = window_delegate_->GetInitiallyFocusedView();
  if (v) {
    v->RequestFocus();
  } else {
    // The window does not get keyboard messages unless we focus it, not sure
    // why.
    SetFocus(GetHWND());
  }
}

void Window::SetInitialBounds(const gfx::Rect& create_bounds) {
  // First we obtain the window's saved show-style and store it. We need to do
  // this here, rather than in Show() because by the time Show() is called,
  // the window's size will have been reset (below) and the saved maximized
  // state will have been lost. Sadly there's no way to tell on Windows when
  // a window is restored from maximized state, so we can't more accurately
  // track maximized state independently of sizing information.
  window_delegate_->GetSavedMaximizedState(&saved_maximized_state_);

  // Restore the window's placement from the controller.
  gfx::Rect saved_bounds(create_bounds.ToRECT());
  if (window_delegate_->GetSavedWindowBounds(&saved_bounds)) {
    // Make sure the bounds are at least the minimum size.
    if (saved_bounds.width() < minimum_size_.cx) {
      saved_bounds.SetRect(saved_bounds.x(), saved_bounds.y(),
                           saved_bounds.right() + minimum_size_.cx -
                              saved_bounds.width(),
                           saved_bounds.bottom());
    }

    if (saved_bounds.height() < minimum_size_.cy) {
      saved_bounds.SetRect(saved_bounds.x(), saved_bounds.y(),
                           saved_bounds.right(),
                           saved_bounds.bottom() + minimum_size_.cy -
                              saved_bounds.height());
    }

    // "Show state" (maximized, minimized, etc) is handled by Show().
    // Don't use SetBounds here. SetBounds constrains to the size of the
    // monitor, but we don't want that when creating a new window as the result
    // of dragging out a tab to create a new window.
    SetWindowPos(NULL, saved_bounds.x(), saved_bounds.y(),
                 saved_bounds.width(), saved_bounds.height(), 0);
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

void Window::InitAlwaysOnTopState() {
  is_always_on_top_ = false;
  if (window_delegate_->GetSavedAlwaysOnTopState(&is_always_on_top_) &&
      is_always_on_top_ != window_delegate_->IsAlwaysOnTop()) {
    AlwaysOnTopChanged();
  }

  if (window_delegate_->HasAlwaysOnTopMenu())
    AddAlwaysOnTopSystemMenuItem();
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
  if (can_maximize) {
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
  if (window_delegate_->AsDialogDelegate())
    window_ex_styles |= WS_EX_DLGMODALFRAME;
  if (window_delegate_->IsAlwaysOnTop())
    window_ex_styles |= WS_EX_TOPMOST;
  return window_ex_styles;
}

void Window::SaveWindowPosition() {
  // The window delegate does the actual saving for us. It seems like (judging
  // by go/crash) that in some circumstances we can end up here after
  // WM_DESTROY, at which point the window delegate is likely gone. So just
  // bail.
  if (!window_delegate_)
    return;

  WINDOWPLACEMENT win_placement = { 0 };
  win_placement.length = sizeof(WINDOWPLACEMENT);

  BOOL r = GetWindowPlacement(GetHWND(), &win_placement);
  DCHECK(r);

  bool maximized = (win_placement.showCmd == SW_SHOWMAXIMIZED);
  CRect window_bounds(win_placement.rcNormalPosition);
  window_delegate_->SaveWindowPlacement(
      gfx::Rect(win_placement.rcNormalPosition), maximized, is_always_on_top_);
}

void Window::LockUpdates() {
  lock_updates_ = true;
  saved_window_style_ = GetWindowLong(GetHWND(), GWL_STYLE);
  SetWindowLong(GetHWND(), GWL_STYLE, saved_window_style_ & ~WS_VISIBLE);
}

void Window::UnlockUpdates() {
  SetWindowLong(GetHWND(), GWL_STYLE, saved_window_style_);
  lock_updates_ = false;
}

void Window::ResetWindowRegion() {
  // A native frame uses the native window region, and we don't want to mess
  // with it.
  if (non_client_view_->UseNativeFrame())
    return;

  // Changing the window region is going to force a paint. Only change the
  // window region if the region really differs.
  HRGN current_rgn = CreateRectRgn(0, 0, 0, 0);
  int current_rgn_result = GetWindowRgn(GetHWND(), current_rgn);

  CRect window_rect;
  GetWindowRect(&window_rect);
  HRGN new_region;
  if (IsMaximized()) {
    HMONITOR monitor = MonitorFromWindow(GetHWND(), MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi;
    mi.cbSize = sizeof mi;
    GetMonitorInfo(monitor, &mi);
    CRect work_rect = mi.rcWork;
    work_rect.OffsetRect(-window_rect.left, -window_rect.top);
    new_region = CreateRectRgnIndirect(&work_rect);
  } else {
    gfx::Path window_mask;
    non_client_view_->GetWindowMask(gfx::Size(window_rect.Width(),
                                              window_rect.Height()),
                                    &window_mask);
    new_region = window_mask.CreateHRGN();
  }

  if (current_rgn_result == ERROR || !EqualRgn(current_rgn, new_region)) {
    // SetWindowRgn takes ownership of the HRGN created by CreateHRGN.
    SetWindowRgn(new_region, TRUE);
  } else {
    DeleteObject(new_region);
  }

  DeleteObject(current_rgn);
}

void Window::ProcessNCMousePress(const CPoint& point, int flags) {
  CPoint temp = point;
  MapWindowPoints(HWND_DESKTOP, GetHWND(), &temp, 1);
  UINT message_flags = 0;
  if ((GetKeyState(VK_CONTROL) & 0x80) == 0x80)
    message_flags |= MK_CONTROL;
  if ((GetKeyState(VK_SHIFT) & 0x80) == 0x80)
    message_flags |= MK_SHIFT;
  message_flags |= flags;
  ProcessMousePressed(temp, message_flags, false, false);
}

void Window::InitClass() {
  static bool initialized = false;
  if (!initialized) {
    resize_cursors_[RC_NORMAL] = LoadCursor(NULL, IDC_ARROW);
    resize_cursors_[RC_VERTICAL] = LoadCursor(NULL, IDC_SIZENS);
    resize_cursors_[RC_HORIZONTAL] = LoadCursor(NULL, IDC_SIZEWE);
    resize_cursors_[RC_NESW] = LoadCursor(NULL, IDC_SIZENESW);
    resize_cursors_[RC_NWSE] = LoadCursor(NULL, IDC_SIZENWSE);
    initialized = true;
  }
}

}  // namespace views

