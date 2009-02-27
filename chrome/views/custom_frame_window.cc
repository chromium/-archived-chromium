// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/views/custom_frame_window.h"

#include "base/gfx/point.h"
#include "base/gfx/size.h"
#include "chrome/common/gfx/chrome_canvas.h"
#include "chrome/common/gfx/path.h"
#include "chrome/common/l10n_util.h"
#include "chrome/common/win_util.h"
#include "chrome/views/client_view.h"
#include "chrome/views/default_non_client_view.h"
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
// associated CustomFrameWindow's lock and unlock functions as it is created
// and destroyed. See documentation in those methods for the technique used.
//
// IMPORTANT: Do not use this scoping object for large scopes or periods of
//            time! IT WILL PREVENT THE WINDOW FROM BEING REDRAWN! (duh).
//
// I would love to hear Raymond Chen's explanation for all this. And maybe a
// list of other messages that this applies to ;-)
class CustomFrameWindow::ScopedRedrawLock {
 public:
  explicit ScopedRedrawLock(CustomFrameWindow* window) : window_(window) {
    window_->LockUpdates();
  }

  ~ScopedRedrawLock() {
    window_->UnlockUpdates();
  }

 private:
  // The window having its style changed.
  CustomFrameWindow* window_;
};

HCURSOR CustomFrameWindow::resize_cursors_[6];

///////////////////////////////////////////////////////////////////////////////
// CustomFrameWindow, public:

CustomFrameWindow::CustomFrameWindow(WindowDelegate* window_delegate)
    : Window(window_delegate),
      is_active_(false),
      lock_updates_(false),
      saved_window_style_(0) {
  InitClass();
  non_client_view_ = new DefaultNonClientView(this);
}

CustomFrameWindow::CustomFrameWindow(WindowDelegate* window_delegate,
                                     NonClientView* non_client_view)
    : Window(window_delegate) {
  InitClass();
  non_client_view_ = non_client_view;
}

CustomFrameWindow::~CustomFrameWindow() {
}

///////////////////////////////////////////////////////////////////////////////
// CustomFrameWindow, Window overrides:

void CustomFrameWindow::Init(HWND parent, const gfx::Rect& bounds) {
  // TODO(beng): (Cleanup) Right now, the only way to specify a different
  //             non-client view is to subclass this object and provide one
  //             by setting this member before calling Init.
  if (!non_client_view_)
    non_client_view_ = new DefaultNonClientView(this);
  Window::Init(parent, bounds);

  ResetWindowRegion();
}

void CustomFrameWindow::UpdateWindowTitle() {
  // Layout winds up causing the title to be re-validated during
  // string measurement.
  non_client_view_->Layout();
  // Must call the base class too so that places like the Task Bar get updated.
  Window::UpdateWindowTitle();
}

void CustomFrameWindow::UpdateWindowIcon() {
  // The icon will be re-validated during painting.
  non_client_view_->SchedulePaint();
  // Call the base class so that places like the Task Bar get updated.
  Window::UpdateWindowIcon();
}

void CustomFrameWindow::EnableClose(bool enable) {
  non_client_view_->EnableClose(enable);
  // Make sure the SysMenu changes to reflect this change as well.
  Window::EnableClose(enable);
}

void CustomFrameWindow::DisableInactiveRendering(bool disable) {
  Window::DisableInactiveRendering(disable);
  non_client_view_->set_paint_as_active(disable);
  if (!disable)
    non_client_view_->SchedulePaint();
}

void CustomFrameWindow::SizeWindowToDefault() {
  gfx::Size pref = client_view()->GetPreferredSize();
  DCHECK(pref.width() > 0 && pref.height() > 0);
  gfx::Size window_size =
      non_client_view_->CalculateWindowSizeForClientSize(pref.width(),
                                                         pref.height());
  win_util::CenterAndSizeWindow(owning_window(), GetHWND(),
                                window_size.ToSIZE(), false);
}

///////////////////////////////////////////////////////////////////////////////
// CustomFrameWindow, WidgetWin overrides:

static void EnableMenuItem(HMENU menu, UINT command, bool enabled) {
  UINT flags = MF_BYCOMMAND | (enabled ? MF_ENABLED : MF_DISABLED | MF_GRAYED);
  EnableMenuItem(menu, command, flags);
}

void CustomFrameWindow::OnInitMenu(HMENU menu) {
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

void CustomFrameWindow::OnMouseLeave() {
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

LRESULT CustomFrameWindow::OnNCActivate(BOOL active) {
  is_active_ = !!active;

  // We can get WM_NCACTIVATE before we're actually visible. If we're not
  // visible, no need to paint.
  if (IsWindowVisible(GetHWND())) {
    non_client_view_->SchedulePaint();
    // We need to force a paint now, as a user dragging a window will block
    // painting operations while the move is in progress.
    PaintNow(root_view_->GetScheduledPaintRect());
  }

  // Defering to our parent as it is important that the NCActivate message gets
  // DefProc'ed or the task bar won't show our process as active.
  // See bug http://crbug.com/4513.
  return Window::OnNCActivate(active);
}

LRESULT CustomFrameWindow::OnNCCalcSize(BOOL mode, LPARAM l_param) {
  // We need to repaint all when the window bounds change.
  return WVR_REDRAW;
}

LRESULT CustomFrameWindow::OnNCHitTest(const CPoint& point) {
  // NC points are in screen coordinates.
  CPoint temp = point;
  MapWindowPoints(HWND_DESKTOP, GetHWND(), &temp, 1);
  return non_client_view_->NonClientHitTest(gfx::Point(temp.x, temp.y));
}

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

void CustomFrameWindow::OnNCPaint(HRGN rgn) {
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

void CustomFrameWindow::OnNCLButtonDown(UINT ht_component,
                                        const CPoint& point) {
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
    default:
      Window::OnNCLButtonDown(ht_component, point);
      /*
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
      break;
  }
}

LRESULT CustomFrameWindow::OnNCUAHDrawCaption(UINT msg, WPARAM w_param,
                                              LPARAM l_param) {
  // See comment in widget_win.h at the definition of WM_NCUAHDRAWCAPTION for
  // an explanation about why we need to handle this message.
  SetMsgHandled(TRUE);
  return 0;
}

LRESULT CustomFrameWindow::OnNCUAHDrawFrame(UINT msg, WPARAM w_param,
                                            LPARAM l_param) {
  // See comment in widget_win.h at the definition of WM_NCUAHDRAWCAPTION for
  // an explanation about why we need to handle this message.
  SetMsgHandled(TRUE);
  return 0;
}

LRESULT CustomFrameWindow::OnSetCursor(HWND window, UINT hittest_code,
                                       UINT message) {
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

LRESULT CustomFrameWindow::OnSetIcon(UINT size_type, HICON new_icon) {
  ScopedRedrawLock lock(this);
  return DefWindowProc(GetHWND(), WM_SETICON, size_type,
                       reinterpret_cast<LPARAM>(new_icon));
}

LRESULT CustomFrameWindow::OnSetText(const wchar_t* text) {
  ScopedRedrawLock lock(this);
  return DefWindowProc(GetHWND(), WM_SETTEXT, NULL,
                       reinterpret_cast<LPARAM>(text));
}

void CustomFrameWindow::OnSize(UINT param, const CSize& size) {
  Window::OnSize(param, size);

  // ResetWindowRegion is going to trigger WM_NCPAINT. By doing it after we've
  // invoked OnSize we ensure the RootView has been layed out.
  ResetWindowRegion();
}

void CustomFrameWindow::OnSysCommand(UINT notification_code, CPoint click) {
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
  Window::OnSysCommand(notification_code, click);
}

///////////////////////////////////////////////////////////////////////////////
// CustomFrameWindow, private:

// static
void CustomFrameWindow::InitClass() {
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

void CustomFrameWindow::LockUpdates() {
  lock_updates_ = true;
  saved_window_style_ = GetWindowLong(GetHWND(), GWL_STYLE);
  SetWindowLong(GetHWND(), GWL_STYLE, saved_window_style_ & ~WS_VISIBLE);
}

void CustomFrameWindow::UnlockUpdates() {
  SetWindowLong(GetHWND(), GWL_STYLE, saved_window_style_);
  lock_updates_ = false;
}

void CustomFrameWindow::ResetWindowRegion() {
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

void CustomFrameWindow::ProcessNCMousePress(const CPoint& point, int flags) {
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

}  // namespace views
