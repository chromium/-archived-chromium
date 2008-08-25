// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <dwmapi.h>

#include "chrome/browser/views/frame/aero_glass_frame.h"

#include "chrome/app/theme/theme_resources.h"
#include "chrome/browser/frame_util.h"
#include "chrome/browser/views/frame/browser_view2.h"
#include "chrome/browser/views/frame/aero_glass_non_client_view.h"
#include "chrome/common/resource_bundle.h"
#include "chrome/views/window_delegate.h"

// static

// The width of the sizing borders.
static const int kResizeBorder = 8;
// By how much the toolbar overlaps with the tab strip.
static const int kToolbarOverlapVertOffset = 5;
// This is the width of the default client edge provided by Windows. In some
// circumstances we provide our own client edge, so we use this width to
// remove it.
static const int kWindowsDWMBevelSize = 2;

///////////////////////////////////////////////////////////////////////////////
// AeroGlassFrame, public:

AeroGlassFrame::AeroGlassFrame(BrowserView2* browser_view)
    : Window(browser_view),
      browser_view_(browser_view),
      frame_initialized_(false) {
  non_client_view_ = new AeroGlassNonClientView(this, browser_view);
  browser_view_->set_frame(this);
}

AeroGlassFrame::~AeroGlassFrame() {
}

void AeroGlassFrame::Init(const gfx::Rect& bounds) {
  Window::Init(NULL, bounds);
}

int AeroGlassFrame::GetMinimizeButtonOffset() const {
  TITLEBARINFOEX titlebar_info;
  titlebar_info.cbSize = sizeof(TITLEBARINFOEX);
  SendMessage(GetHWND(), WM_GETTITLEBARINFOEX, 0, (WPARAM)&titlebar_info);

  RECT wr;
  GetWindowRect(&wr);

  return wr.right - titlebar_info.rgrect[2].left;
}

///////////////////////////////////////////////////////////////////////////////
// AeroGlassFrame, BrowserFrame implementation:

gfx::Rect AeroGlassFrame::GetWindowBoundsForClientBounds(
    const gfx::Rect& client_bounds) {
  RECT rect = client_bounds.ToRECT();
  AdjustWindowRectEx(&rect, window_style(), FALSE, window_ex_style());
  return gfx::Rect(rect);
}

void AeroGlassFrame::SizeToContents(const gfx::Rect& contents_bounds) {
  // TODO(beng): implement me.
}

gfx::Rect AeroGlassFrame::GetBoundsForTabStrip(TabStrip* tabstrip) const {
  return GetAeroGlassNonClientView()->GetBoundsForTabStrip(tabstrip);
}

ChromeViews::Window* AeroGlassFrame::GetWindow() {
  return this;
}

///////////////////////////////////////////////////////////////////////////////
// AeroGlassFrame, ChromeViews::Window overrides:

void AeroGlassFrame::UpdateWindowIcon() {
  Window::UpdateWindowIcon();
  // TODO(beng): do something in the non-client view when this builds on Vista.
}

///////////////////////////////////////////////////////////////////////////////
// AeroGlassFrame, ChromeViews::HWNDViewContainer implementation:

void AeroGlassFrame::OnInitMenuPopup(HMENU menu, UINT position,
                                     BOOL is_system_menu) {
  browser_view_->PrepareToRunSystemMenu(menu);
}

void AeroGlassFrame::OnEndSession(BOOL ending, UINT logoff) {
  FrameUtil::EndSession();
}

void AeroGlassFrame::OnExitMenuLoop(bool is_track_popup_menu) {
  browser_view_->SystemMenuEnded();
}

LRESULT AeroGlassFrame::OnMouseActivate(HWND window, UINT hittest_code,
                                        UINT message) {
  return browser_view_->ActivateAppModalDialog() ? MA_NOACTIVATEANDEAT
                                                 : MA_ACTIVATE;
}

void AeroGlassFrame::OnMove(const CPoint& point) {
  browser_view_->WindowMoved();
}

void AeroGlassFrame::OnMoving(UINT param, const RECT* new_bounds) {
  browser_view_->WindowMoved();
}

LRESULT AeroGlassFrame::OnNCActivate(BOOL active) {
  if (browser_view_->ActivateAppModalDialog())
    return TRUE;

  if (!frame_initialized_) {
    ::SetWindowPos(GetHWND(), NULL, 0, 0, 0, 0,
                   SWP_NOSIZE | SWP_NOMOVE | SWP_FRAMECHANGED);
    UpdateDWMFrame();
    frame_initialized_ = true;
  }
  browser_view_->ActivationChanged(!!active);
  return TRUE;
}

LRESULT AeroGlassFrame::OnNCCalcSize(BOOL mode, LPARAM l_param) {
  // By default the client side is set to the window size which is what
  // we want.
  if (browser_view_->IsToolbarVisible() && mode == TRUE) {
    // To be on the safe side and avoid side-effects, we only adjust the client
    // size to non-standard values when we must - i.e. when we're showing a
    // TabStrip.
    if (browser_view_->IsTabStripVisible()) {
      // Calculate new NCCALCSIZE_PARAMS based on custom NCA inset.
      NCCALCSIZE_PARAMS* params = reinterpret_cast<NCCALCSIZE_PARAMS*>(l_param);

      // Hack necessary to stop black background flicker, we cut out
      // resizeborder here to save us from having to do too much
      // addition and subtraction in Layout(). We don't cut off the
      // top + titlebar as that prevents the window controls from
      // highlighting.
      params->rgrc[0].left += kResizeBorder;
      params->rgrc[0].right -= kResizeBorder;
      params->rgrc[0].bottom -= kResizeBorder;

      SetMsgHandled(TRUE);
    } else {
      // We don't adjust the client size for detached popups, so we need to
      // tell Windows we didn't handle the message here so that it doesn't
      // screw up the non-client area.
      SetMsgHandled(FALSE);
    }

    // We need to reset the frame, as Vista resets it whenever it changes
    // composition modes (and NCCALCSIZE is the closest thing we get to
    // a reliable message about the change).
    UpdateDWMFrame();

    return 0;
  }
  SetMsgHandled(FALSE);
  return 0;
}

LRESULT AeroGlassFrame::OnNCHitTest(const CPoint& pt) {
  LRESULT result;
  if (DwmDefWindowProc(GetHWND(), WM_NCHITTEST, 0, MAKELPARAM(pt.x, pt.y),
                       &result)) {
    return result;
  }
  return Window::OnNCHitTest(pt);
}

///////////////////////////////////////////////////////////////////////////////
// AeroGlassFrame, ChromeViews::HWNDViewContainer overrides:

bool AeroGlassFrame::AcceleratorPressed(
    ChromeViews::Accelerator* accelerator) {
  return browser_view_->AcceleratorPressed(*accelerator);
}

bool AeroGlassFrame::GetAccelerator(int cmd_id,
                                    ChromeViews::Accelerator* accelerator) {
  return browser_view_->GetAccelerator(cmd_id, accelerator);
}

///////////////////////////////////////////////////////////////////////////////
// AeroGlassFrame, private:

void AeroGlassFrame::UpdateDWMFrame() {
  // Nothing to do yet.
  if (!client_view())
    return;

  // TODO(beng): when TabStrip is hooked up, obtain this offset from its
  //             bounds.
  int toolbar_y = 36;

  // We only adjust the DWM's glass rendering when we're a browser window or a
  // detached popup. App windows get the standard client edge.
  if (browser_view_->IsTabStripVisible() ||
      browser_view_->IsToolbarVisible()) {
    // By default, we just want to adjust the glass by the width of the inner
    // bevel that aero renders to demarcate the client area. We supply our own
    // client edge for the browser window and detached popups, so we don't want
    // to show the default one.
    int client_edge_left_width = kWindowsDWMBevelSize;
    int client_edge_right_width = kWindowsDWMBevelSize;
    int client_edge_bottom_height = kWindowsDWMBevelSize;
    int client_edge_top_height = kWindowsDWMBevelSize;
    if (browser_view_->IsTabStripVisible()) {
      client_edge_top_height =
          client_view()->GetY() + kToolbarOverlapVertOffset + toolbar_y;
    }

    // Now poke the DWM.
    MARGINS margins = { client_edge_left_width, client_edge_right_width,
                        client_edge_top_height, client_edge_bottom_height };
    // Note: we don't use DwmEnableBlurBehindWindow because any region not
    // included in the glass region is composited source over. This means
    // that anything drawn directly with GDI appears fully transparent.
    DwmExtendFrameIntoClientArea(GetHWND(), &margins);
  }
}

AeroGlassNonClientView* AeroGlassFrame::GetAeroGlassNonClientView() const {
  // We can safely assume that this conversion is true.
  return static_cast<AeroGlassNonClientView*>(non_client_view_);
}

