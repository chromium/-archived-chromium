// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/frame/browser_frame.h"

#include <dwmapi.h>
#include <shellapi.h>

#include "chrome/browser/browser_list.h"
#include "chrome/browser/views/frame/browser_view.h"
#include "chrome/browser/views/frame/glass_browser_frame_view.h"
#include "chrome/browser/views/frame/opaque_browser_frame_view.h"
#include "chrome/common/resource_bundle.h"
#include "chrome/common/win_util.h"
#include "chrome/views/window_delegate.h"
#include "grit/theme_resources.h"

// static
static const int kClientEdgeThickness = 3;

///////////////////////////////////////////////////////////////////////////////
// BrowserFrame, public:

BrowserFrame::BrowserFrame(BrowserView* browser_view)
    : Window(browser_view),
      browser_view_(browser_view),
      frame_initialized_(false) {
  browser_view_->set_frame(this);
  non_client_view_->SetFrameView(CreateFrameViewForWindow());
  // Don't focus anything on creation, selecting a tab will set the focus.
  set_focus_on_creation(false);
}

BrowserFrame::~BrowserFrame() {
}

void BrowserFrame::Init() {
  Window::Init(NULL, gfx::Rect());
}

int BrowserFrame::GetMinimizeButtonOffset() const {
  TITLEBARINFOEX titlebar_info;
  titlebar_info.cbSize = sizeof(TITLEBARINFOEX);
  SendMessage(GetHWND(), WM_GETTITLEBARINFOEX, 0, (WPARAM)&titlebar_info);

  CPoint minimize_button_corner(titlebar_info.rgrect[2].left,
                                titlebar_info.rgrect[2].top);
  MapWindowPoints(HWND_DESKTOP, GetHWND(), &minimize_button_corner, 1);

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
  if (non_client_view_->UseNativeFrame() && !frame_initialized_) {
    if (browser_view_->IsBrowserTypeNormal()) {
      ::SetWindowPos(GetHWND(), NULL, 0, 0, 0, 0,
                     SWP_NOSIZE | SWP_NOMOVE | SWP_FRAMECHANGED);
      UpdateDWMFrame();
    }
    frame_initialized_ = true;
  }
  browser_view_->ActivationChanged(!!active);
  return Window::OnNCActivate(active);
}

LRESULT BrowserFrame::OnNCCalcSize(BOOL mode, LPARAM l_param) {
  // We don't adjust the client area unless we're a tabbed browser window and
  // are using the native frame.
  if (!non_client_view_->UseNativeFrame() ||
      !browser_view_->IsBrowserTypeNormal() || !mode) {
    return Window::OnNCCalcSize(mode, l_param);
  }

  // In fullscreen mode, we make the whole window client area.
  if (!browser_view_->IsFullscreen()) {
    NCCALCSIZE_PARAMS* params = reinterpret_cast<NCCALCSIZE_PARAMS*>(l_param);
    int border_thickness = GetSystemMetrics(SM_CXSIZEFRAME);
    params->rgrc[0].left += (border_thickness - kClientEdgeThickness);
    params->rgrc[0].right -= (border_thickness - kClientEdgeThickness);
    params->rgrc[0].bottom -= (border_thickness - kClientEdgeThickness);
  }

  UpdateDWMFrame();

  SetMsgHandled(TRUE);
  return 0;
}

LRESULT BrowserFrame::OnNCHitTest(const CPoint& pt) {
  // Only do DWM hit-testing when we are using the native frame.
  if (non_client_view_->UseNativeFrame()) {
    LRESULT result;
    if (DwmDefWindowProc(GetHWND(), WM_NCHITTEST, 0, MAKELPARAM(pt.x, pt.y),
                         &result)) {
      return result;
    }
  }
  return Window::OnNCHitTest(pt);
}

///////////////////////////////////////////////////////////////////////////////
// BrowserFrame, views::CustomFrameWindow overrides:

int BrowserFrame::GetShowState() const {
  return browser_view_->GetShowState();
}

views::NonClientFrameView* BrowserFrame::CreateFrameViewForWindow() {
  if (non_client_view_->UseNativeFrame())
    browser_frame_view_ = new GlassBrowserFrameView(this, browser_view_);
  else
    browser_frame_view_ = new OpaqueBrowserFrameView(this, browser_view_);
  return browser_frame_view_;
}

void BrowserFrame::UpdateFrameAfterFrameChange() {
  Window::UpdateFrameAfterFrameChange();
  UpdateDWMFrame();
}

///////////////////////////////////////////////////////////////////////////////
// BrowserFrame, private:

void BrowserFrame::UpdateDWMFrame() {
  // Nothing to do yet.
  if (!client_view() || !browser_view_->IsBrowserTypeNormal())
    return;

  // In fullscreen mode, we don't extend glass into the client area at all,
  // because the GDI-drawn text in the web content composited over it will
  // become semi-transparent over any glass area.
  MARGINS margins = { 0 };
  if (!browser_view_->IsFullscreen()) {
    margins.cxLeftWidth = kClientEdgeThickness + 1;
    margins.cxRightWidth = kClientEdgeThickness + 1;
    margins.cyTopHeight =
        GetBoundsForTabStrip(browser_view_->tabstrip()).bottom();
    margins.cyBottomHeight = kClientEdgeThickness + 1;
  }
  DwmExtendFrameIntoClientArea(GetHWND(), &margins);
}
