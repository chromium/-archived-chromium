// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/frame/browser_frame.h"

#include <dwmapi.h>
#include <shellapi.h>

#include "chrome/browser/browser_list.h"
#include "chrome/browser/views/frame/browser_root_view.h"
#include "chrome/browser/views/frame/browser_view.h"
#include "chrome/browser/views/frame/glass_browser_frame_view.h"
#include "chrome/browser/views/frame/opaque_browser_frame_view.h"
#include "chrome/common/resource_bundle.h"
#include "chrome/common/win_util.h"
#include "chrome/views/window/window_delegate.h"
#include "grit/theme_resources.h"

// static
static const int kClientEdgeThickness = 3;

///////////////////////////////////////////////////////////////////////////////
// BrowserFrame, public:

BrowserFrame::BrowserFrame(BrowserView* browser_view)
    : WindowWin(browser_view),
      browser_view_(browser_view),
      frame_initialized_(false) {
  browser_view_->set_frame(this);
  GetNonClientView()->SetFrameView(CreateFrameViewForWindow());
  // Don't focus anything on creation, selecting a tab will set the focus.
  set_focus_on_creation(false);
}

BrowserFrame::~BrowserFrame() {
}

void BrowserFrame::Init() {
  WindowWin::Init(NULL, gfx::Rect());
}

int BrowserFrame::GetMinimizeButtonOffset() const {
  TITLEBARINFOEX titlebar_info;
  titlebar_info.cbSize = sizeof(TITLEBARINFOEX);
  SendMessage(GetNativeView(), WM_GETTITLEBARINFOEX, 0, (WPARAM)&titlebar_info);

  CPoint minimize_button_corner(titlebar_info.rgrect[2].left,
                                titlebar_info.rgrect[2].top);
  MapWindowPoints(HWND_DESKTOP, GetNativeView(), &minimize_button_corner, 1);

  return minimize_button_corner.x;
}

gfx::Rect BrowserFrame::GetBoundsForTabStrip(TabStrip* tabstrip) const {
  return browser_frame_view_->GetBoundsForTabStrip(tabstrip);
}

void BrowserFrame::UpdateThrobber(bool running) {
  browser_frame_view_->UpdateThrobber(running);
}

///////////////////////////////////////////////////////////////////////////////
// BrowserFrame, views::WidgetWin overrides:

bool BrowserFrame::AcceleratorPressed(views::Accelerator* accelerator) {
  return browser_view_->AcceleratorPressed(*accelerator);
}

bool BrowserFrame::GetAccelerator(int cmd_id, views::Accelerator* accelerator) {
  return browser_view_->GetAccelerator(cmd_id, accelerator);
}

void BrowserFrame::OnEndSession(BOOL ending, UINT logoff) {
  BrowserList::WindowsSessionEnding();
}

void BrowserFrame::OnEnterSizeMove() {
  browser_view_->WindowMoveOrResizeStarted();
}

void BrowserFrame::OnInitMenuPopup(HMENU menu, UINT position,
                                     BOOL is_system_menu) {
  browser_view_->PrepareToRunSystemMenu(menu);
}

LRESULT BrowserFrame::OnMouseActivate(HWND window, UINT hittest_code,
                                        UINT message) {
  return browser_view_->ActivateAppModalDialog() ? MA_NOACTIVATEANDEAT
                                                 : MA_ACTIVATE;
}

void BrowserFrame::OnMove(const CPoint& point) {
  browser_view_->WindowMoved();
}

void BrowserFrame::OnMoving(UINT param, const RECT* new_bounds) {
  browser_view_->WindowMoved();
}

LRESULT BrowserFrame::OnNCActivate(BOOL active) {
  if (browser_view_->ActivateAppModalDialog())
    return TRUE;

  // Perform first time initialization of the DWM frame insets, only if we're
  // using the native frame.
  if (GetNonClientView()->UseNativeFrame() && !frame_initialized_) {
    if (browser_view_->IsBrowserTypeNormal()) {
      ::SetWindowPos(GetNativeView(), NULL, 0, 0, 0, 0,
                     SWP_NOSIZE | SWP_NOMOVE | SWP_FRAMECHANGED);
      UpdateDWMFrame();
    }
    frame_initialized_ = true;
  }
  browser_view_->ActivationChanged(!!active);
  return WindowWin::OnNCActivate(active);
}

LRESULT BrowserFrame::OnNCCalcSize(BOOL mode, LPARAM l_param) {
  // We don't adjust the client area unless we're a tabbed browser window and
  // are using the native frame.
  if (!GetNonClientView()->UseNativeFrame() ||
      !browser_view_->IsBrowserTypeNormal()) {
    return WindowWin::OnNCCalcSize(mode, l_param);
  }

  RECT* client_rect = mode ?
      &reinterpret_cast<NCCALCSIZE_PARAMS*>(l_param)->rgrc[0] :
      reinterpret_cast<RECT*>(l_param);
  int border_thickness = 0;
  if (browser_view_->IsMaximized()) {
    // Make the maximized mode client rect fit the screen exactly, by
    // subtracting the border Windows automatically adds for maximized mode.
    border_thickness = GetSystemMetrics(SM_CXSIZEFRAME);
    // Find all auto-hide taskbars along the screen edges and adjust in by the
    // thickness of the auto-hide taskbar on each such edge, so the window isn't
    // treated as a "fullscreen app", which would cause the taskbars to
    // disappear.
    HMONITOR monitor = MonitorFromWindow(GetNativeView(),
                                         MONITOR_DEFAULTTONEAREST);
    if (win_util::EdgeHasAutoHideTaskbar(ABE_LEFT, monitor))
      client_rect->left += win_util::kAutoHideTaskbarThicknessPx;
    if (win_util::EdgeHasAutoHideTaskbar(ABE_RIGHT, monitor))
      client_rect->right -= win_util::kAutoHideTaskbarThicknessPx;
    if (win_util::EdgeHasAutoHideTaskbar(ABE_BOTTOM, monitor)) {
      client_rect->bottom -= win_util::kAutoHideTaskbarThicknessPx;
    } else if (win_util::EdgeHasAutoHideTaskbar(ABE_TOP, monitor)) {
      // Tricky bit.  Due to a bug in DwmDefWindowProc()'s handling of
      // WM_NCHITTEST, having any nonclient area atop the window causes the
      // caption buttons to draw onscreen but not respond to mouse hover/clicks.
      // So for a taskbar at the screen top, we can't push the client_rect->top
      // down; instead, we move the bottom up by one pixel, which is the
      // smallest change we can make and still get a client area less than the
      // screen size. This is visibly ugly, but there seems to be no better
      // solution.
      --client_rect->bottom;
    }
  } else if (!browser_view_->IsFullscreen()) {
    // We draw our own client edge over part of the default frame would be.
    border_thickness = GetSystemMetrics(SM_CXSIZEFRAME) - kClientEdgeThickness;
  }
  client_rect->left += border_thickness;
  client_rect->right -= border_thickness;
  client_rect->bottom -= border_thickness;

  UpdateDWMFrame();

  // Non-client metrics such as the window control positions may change as a
  // result of us processing this message so we need to re-layout the frame view
  // which may position items (such as the distributor logo) based on these
  // metrics. We only do this once the non-client view has been properly
  // initialized and added to the view hierarchy.
  views::NonClientView* non_client_view = GetNonClientView();
  if (non_client_view && non_client_view->GetParent())
    non_client_view->LayoutFrameView();

  // We'd like to return WVR_REDRAW in some cases here, but because we almost
  // always have nonclient area (except in fullscreen mode, where it doesn't
  // matter), we can't.  See comments in window.cc:OnNCCalcSize() for more info.
  return 0;
}

LRESULT BrowserFrame::OnNCHitTest(const CPoint& pt) {
  // Only do DWM hit-testing when we are using the native frame.
  if (GetNonClientView()->UseNativeFrame()) {
    LRESULT result;
    if (DwmDefWindowProc(GetNativeView(), WM_NCHITTEST, 0,
                         MAKELPARAM(pt.x, pt.y), &result)) {
      return result;
    }
  }
  return WindowWin::OnNCHitTest(pt);
}

///////////////////////////////////////////////////////////////////////////////
// BrowserFrame, views::CustomFrameWindow overrides:

int BrowserFrame::GetShowState() const {
  return browser_view_->GetShowState();
}

views::NonClientFrameView* BrowserFrame::CreateFrameViewForWindow() {
  if (GetNonClientView()->UseNativeFrame())
    browser_frame_view_ = new GlassBrowserFrameView(this, browser_view_);
  else
    browser_frame_view_ = new OpaqueBrowserFrameView(this, browser_view_);
  return browser_frame_view_;
}

void BrowserFrame::UpdateFrameAfterFrameChange() {
  WindowWin::UpdateFrameAfterFrameChange();
  UpdateDWMFrame();
}


views::RootView* BrowserFrame::CreateRootView() {
  return new BrowserRootView(this);
}

///////////////////////////////////////////////////////////////////////////////
// BrowserFrame, private:

void BrowserFrame::UpdateDWMFrame() {
  // Nothing to do yet.
  if (!GetClientView() || !browser_view_->IsBrowserTypeNormal())
    return;

  // In fullscreen mode, we don't extend glass into the client area at all,
  // because the GDI-drawn text in the web content composited over it will
  // become semi-transparent over any glass area.
  MARGINS margins = { 0 };
  if (browser_view_->CanCurrentlyResize()) {
    margins.cxLeftWidth = kClientEdgeThickness + 1;
    margins.cxRightWidth = kClientEdgeThickness + 1;
    margins.cyBottomHeight = kClientEdgeThickness + 1;
  }
  // In maximized mode, we only have a titlebar strip of glass, no side/bottom
  // borders.
  if (!browser_view_->IsFullscreen()) {
    margins.cyTopHeight =
        GetBoundsForTabStrip(browser_view_->tabstrip()).bottom();
  }
  DwmExtendFrameIntoClientArea(GetNativeView(), &margins);
}
