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

#include <dwmapi.h>

#include "chrome/browser/views/frame/aero_glass_frame.h"

#include "chrome/app/theme/theme_resources.h"
#include "chrome/browser/views/frame/browser_view2.h"
#include "chrome/browser/views/frame/aero_glass_non_client_view.h"
#include "chrome/common/resource_bundle.h"
#include "chrome/views/window_delegate.h"

// static
static const int kResizeBorder = 5;

// By how much the toolbar overlaps with the tab strip.
static const int kToolbarOverlapVertOffset = 3;

// The DWM puts a light border around the client area - we need to
// take this border size into account when we reduce its size so that
// we don't draw our content border dropshadow images over the top.
static const int kDwmBorderSize = 1;

///////////////////////////////////////////////////////////////////////////////
// AeroGlassFrame, public:

AeroGlassFrame::AeroGlassFrame(BrowserView2* browser_view)
    : Window(browser_view),
      browser_view_(browser_view),
      frame_initialized_(false) {
  non_client_view_ = new AeroGlassNonClientView(this);
  browser_view_->set_frame(this);
}

AeroGlassFrame::~AeroGlassFrame() {
}

void AeroGlassFrame::Init(const gfx::Rect& bounds) {
  Window::Init(NULL, bounds);
}

bool AeroGlassFrame::IsTabStripVisible() const {
  return browser_view_->IsTabStripVisible();
}

bool AeroGlassFrame::IsToolbarVisible() const {
  return browser_view_->IsToolbarVisible();
}

gfx::Rect AeroGlassFrame::GetToolbarBounds() const {
  return browser_view_->GetToolbarBounds();
}

gfx::Rect AeroGlassFrame::GetContentsBounds() const {
  return browser_view_->GetClientAreaBounds();
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
  return gfx::Rect();
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
// AeroGlassFrame, ChromeViews::HWNDViewContainer implementation:

LRESULT AeroGlassFrame::OnNCActivate(BOOL active) {
  if (!frame_initialized_) {
    ::SetWindowPos(GetHWND(), NULL, 0, 0, 0, 0,
                   SWP_NOSIZE | SWP_NOMOVE | SWP_FRAMECHANGED);
    UpdateDWMFrame();
    frame_initialized_ = true;
  }
  return TRUE;
}

LRESULT AeroGlassFrame::OnNCCalcSize(BOOL mode, LPARAM l_param) {
  // By default the client side is set to the window size which is what
  // we want.
  if (mode == TRUE) {
    // Calculate new NCCALCSIZE_PARAMS based on custom NCA inset.
    NCCALCSIZE_PARAMS *pncsp = reinterpret_cast<NCCALCSIZE_PARAMS*>(l_param);

    // Hack necessary to stop black background flicker, we cut out
    // resizeborder here to save us from having to do too much
    // addition and subtraction in Layout(). We don't cut off the
    // top + titlebar as that prevents the window controls from
    // highlighting.
    pncsp->rgrc[0].left   = pncsp->rgrc[0].left   + kResizeBorder;
    pncsp->rgrc[0].right  = pncsp->rgrc[0].right  - kResizeBorder;
    pncsp->rgrc[0].bottom = pncsp->rgrc[0].bottom - kResizeBorder;

    // We need to reset the frame, as Vista resets it whenever it changes
    // composition modes (and NCCALCSIZE is the closest thing we get to
    // a reliable message about the change).
    UpdateDWMFrame();

    SetMsgHandled(TRUE);
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

void AeroGlassFrame::OnMove(const CPoint& point) {
  browser_view_->WindowMoved();
}

void AeroGlassFrame::OnMoving(UINT param, const RECT* new_bounds) {
  browser_view_->WindowMoved();
}

///////////////////////////////////////////////////////////////////////////////
// AeroGlassFrame, private:

void AeroGlassFrame::UpdateDWMFrame() {
  // Nothing to do yet.
  if (!client_view())
    return;

  // Note: we don't use DwmEnableBlurBehindWindow because any region not
  // included in the glass region is composited source over. This means
  // that anything drawn directly with GDI appears fully transparent.
  //
  // We want this region to extend past our content border images, as they
  // may be partially-transparent.
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  SkBitmap left_edge = *rb.GetBitmapNamed(IDR_CONTENT_TOP_LEFT_CORNER);
  SkBitmap bottom_edge = *rb.GetBitmapNamed(IDR_CONTENT_BOTTOM_CENTER);
  SkBitmap right_edge = *rb.GetBitmapNamed(IDR_CONTENT_TOP_RIGHT_CORNER);

  // TODO(beng): when TabStrip is hooked up, obtain this offset from its
  //             bounds.
  int toolbar_y = 36;
  MARGINS margins = { kDwmBorderSize + left_edge.width(),
                      kDwmBorderSize + right_edge.width(),
                      kDwmBorderSize + client_view()->GetY() +
                          kToolbarOverlapVertOffset + toolbar_y,
                      kDwmBorderSize + bottom_edge.height() };
  DwmExtendFrameIntoClientArea(GetHWND(), &margins);
}

AeroGlassNonClientView* AeroGlassFrame::GetAeroGlassNonClientView() const {
  // We can safely assume that this conversion is true.
  return static_cast<AeroGlassNonClientView*>(non_client_view_);
}
