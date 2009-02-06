// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/frame/aero_glass_frame.h"

#include <dwmapi.h>

#include "chrome/app/theme/theme_resources.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/views/frame/browser_view.h"
#include "chrome/browser/views/frame/aero_glass_non_client_view.h"
#include "chrome/common/resource_bundle.h"
#include "chrome/views/window_delegate.h"

// static

// The width of the client edge to the left and right of the window.
static const int kClientEdgeWidth = 3;
// The height of the client edge to the bottom of the window.
static const int kClientEdgeHeight = 2;

HICON AeroGlassFrame::throbber_icons_[AeroGlassFrame::kThrobberIconCount];

///////////////////////////////////////////////////////////////////////////////
// AeroGlassFrame, public:

AeroGlassFrame::AeroGlassFrame(BrowserView* browser_view)
    : Window(browser_view),
      browser_view_(browser_view),
      frame_initialized_(false),
      throbber_running_(false),
      throbber_frame_(0) {
  non_client_view_ = new AeroGlassNonClientView(this, browser_view);
  browser_view_->set_frame(this);

  if (window_delegate()->ShouldShowWindowIcon())
    InitThrobberIcons();
}

AeroGlassFrame::~AeroGlassFrame() {
}

void AeroGlassFrame::Init() {
  Window::Init(NULL, gfx::Rect());
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

gfx::Rect AeroGlassFrame::GetBoundsForTabStrip(TabStrip* tabstrip) const {
  return GetAeroGlassNonClientView()->GetBoundsForTabStrip(tabstrip);
}

void AeroGlassFrame::UpdateThrobber(bool running) {
  if (throbber_running_) {
    if (running) {
      DisplayNextThrobberFrame();
    } else {
      StopThrobber();
    }
  } else if (running) {
    StartThrobber();
  }
}

views::Window* AeroGlassFrame::GetWindow() {
  return this;
}

///////////////////////////////////////////////////////////////////////////////
// AeroGlassFrame, views::WidgetWin overrides:

bool AeroGlassFrame::AcceleratorPressed(views::Accelerator* accelerator) {
  return browser_view_->AcceleratorPressed(*accelerator);
}

bool AeroGlassFrame::GetAccelerator(int cmd_id,
                                    views::Accelerator* accelerator) {
  return browser_view_->GetAccelerator(cmd_id, accelerator);
}

void AeroGlassFrame::OnInitMenuPopup(HMENU menu, UINT position,
                                     BOOL is_system_menu) {
  browser_view_->PrepareToRunSystemMenu(menu);
}

void AeroGlassFrame::OnEndSession(BOOL ending, UINT logoff) {
  BrowserList::WindowsSessionEnding();
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
    if (browser_view_->IsTabStripVisible()) {
      ::SetWindowPos(GetHWND(), NULL, 0, 0, 0, 0,
                     SWP_NOSIZE | SWP_NOMOVE | SWP_FRAMECHANGED);
      UpdateDWMFrame();
    }
    frame_initialized_ = true;
  }
  browser_view_->ActivationChanged(!!active);
  SetMsgHandled(false);
  return TRUE;
}

LRESULT AeroGlassFrame::OnNCCalcSize(BOOL mode, LPARAM l_param) {
  if (!browser_view_->IsTabStripVisible() || !mode) {
    SetMsgHandled(FALSE);
    return 0;
  }

  NCCALCSIZE_PARAMS* params = reinterpret_cast<NCCALCSIZE_PARAMS*>(l_param);
  int border_thickness = GetSystemMetrics(SM_CXSIZEFRAME);
  params->rgrc[0].left += (border_thickness - kClientEdgeWidth);
  params->rgrc[0].right -= (border_thickness - kClientEdgeWidth);
  params->rgrc[0].bottom -= (border_thickness - kClientEdgeHeight);

  UpdateDWMFrame();

  SetMsgHandled(TRUE);
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
// AeroGlassFrame, views::CustomFrameWindow overrides:

int AeroGlassFrame::GetShowState() const {
  return browser_view_->GetShowState();
}

///////////////////////////////////////////////////////////////////////////////
// AeroGlassFrame, private:

void AeroGlassFrame::UpdateDWMFrame() {
  // Nothing to do yet.
  if (!client_view())
    return;

  MARGINS margins = { kClientEdgeWidth + 1,
                      kClientEdgeWidth + 1,
                      GetBoundsForTabStrip(browser_view_->tabstrip()).bottom(),
                      kClientEdgeHeight + 1 };
  // Note: we don't use DwmEnableBlurBehindWindow because any region not
  // included in the glass region is composited source over. This means
  // that anything drawn directly with GDI appears fully transparent.
  DwmExtendFrameIntoClientArea(GetHWND(), &margins);
}

AeroGlassNonClientView* AeroGlassFrame::GetAeroGlassNonClientView() const {
  // We can safely assume that this conversion is true.
  return static_cast<AeroGlassNonClientView*>(non_client_view_);
}

void AeroGlassFrame::StartThrobber() {
  if (!throbber_running_) {
    throbber_running_ = true;
    throbber_frame_ = 0;
    InitThrobberIcons();
    ::SendMessage(GetHWND(), WM_SETICON, static_cast<WPARAM>(ICON_SMALL),
                  reinterpret_cast<LPARAM>(throbber_icons_[throbber_frame_]));
  }
}

void AeroGlassFrame::StopThrobber() {
  if (throbber_running_)
    throbber_running_ = false;
}

void AeroGlassFrame::DisplayNextThrobberFrame() {
  throbber_frame_ = (throbber_frame_ + 1) % kThrobberIconCount;
  ::SendMessage(GetHWND(), WM_SETICON, static_cast<WPARAM>(ICON_SMALL),
                reinterpret_cast<LPARAM>(throbber_icons_[throbber_frame_]));
}

// static
void AeroGlassFrame::InitThrobberIcons() {
  static bool initialized = false;
  if (!initialized) {
    ResourceBundle &rb = ResourceBundle::GetSharedInstance();
    for (int i = 0; i < kThrobberIconCount; ++i) {
      throbber_icons_[i] = rb.LoadThemeIcon(IDR_THROBBER_01 + i);
      DCHECK(throbber_icons_[i]);
    }
    initialized = true;
  }
}
