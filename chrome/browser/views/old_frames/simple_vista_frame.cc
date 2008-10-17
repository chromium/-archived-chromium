// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/old_frames/simple_vista_frame.h"

#include "chrome/app/theme/theme_resources.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/tab_contents.h"
#include "chrome/browser/views/tab_contents_container_view.h"
#include "chrome/browser/web_app.h"
#include "chrome/browser/web_app_icon_manager.h"
#include "chrome/browser/web_contents.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/gfx/icon_util.h"
#include "chrome/common/l10n_util.h"
#include "chrome/common/resource_bundle.h"
#include "net/base/net_util.h"

#include "chromium_strings.h"
#include "generated_resources.h"

// Number of frames for our throbber.
static const int kThrobberIconCount = 24;

// How outdented the location bar should be (so that the DWM client area
// border masks the location bar frame).
static const int kLocationBarOutdent = 2;
// Spacing between the location bar and the content area.
static const int kLocationBarSpacing = 1;

// Each throbber frame.
static HICON g_throbber_icons[kThrobberIconCount];

static bool g_throbber_initialized = false;

static void InitThrobberIcons() {
  if (!g_throbber_initialized) {
    ResourceBundle &rb = ResourceBundle::GetSharedInstance();
    for (int i = 0; i < kThrobberIconCount; ++i) {
      g_throbber_icons[i] = rb.LoadThemeIcon(IDR_THROBBER_01 + i);
      DCHECK(g_throbber_icons[i]);
    }
    g_throbber_initialized = true;
  }
}

// static
SimpleVistaFrame* SimpleVistaFrame::CreateFrame(const gfx::Rect& bounds,
                                                Browser* browser) {
  SimpleVistaFrame* instance = new SimpleVistaFrame(browser);
  instance->Create(NULL, bounds.ToRECT(),
                   l10n_util::GetString(IDS_PRODUCT_NAME).c_str());
  instance->InitAfterHWNDCreated();
  instance->SetIsOffTheRecord(browser->profile()->IsOffTheRecord());
  views::FocusManager::CreateFocusManager(instance->m_hWnd,
                                          instance->GetRootView());
  return instance;
}

SimpleVistaFrame::SimpleVistaFrame(Browser* browser)
    : VistaFrame(browser),
      throbber_running_(false),
      throbber_frame_(0),
      location_bar_(NULL) {
}

SimpleVistaFrame::~SimpleVistaFrame() {
}

void SimpleVistaFrame::Init() {
  VistaFrame::Init();
  location_bar_ = new LocationBarView(browser_->profile(),
                                      browser_->controller(),
                                      browser_->toolbar_model(),
                                      this, true);
  frame_view_->AddChildView(location_bar_);
  location_bar_->Init();

  // Constrained popups that were unconstrained will need to set up a
  // throbber.
  UpdateTitleBar();
}

void SimpleVistaFrame::SetWindowTitle(const std::wstring& title) {
  std::wstring t;
  if (browser_->IsApplication()) {
    t = title;
  } else {
    t = Browser::ComputePopupTitle(browser_->GetSelectedTabContents()->GetURL(),
                                   title);
  }

  VistaFrame::SetWindowTitle(t);
  SetWindowText(t.c_str());
  UpdateLocationBar();
}

void SimpleVistaFrame::ShowTabContents(TabContents* selected_contents) {
  VistaFrame::ShowTabContents(selected_contents);
  icon_manager_->SetContents(selected_contents);
  UpdateLocationBar();
}

void SimpleVistaFrame::SizeToContents(const gfx::Rect& contents_bounds) {
  // First we need to ensure everything has an initial size. Currently, the
  // window has the wrong size, but that's OK, doing this will allow us to
  // figure out how big all the UI bits are.
  Layout();

  // These calculations are a copy from VistaFrame and we used to just use
  // that implementation. The problem is that we overload Layout() which
  // then references our location_bar_, which doesn't get compensated for
  // in VistaFrame::SizeToContents().
  CRect window_bounds, client_bounds;
  GetBounds(&window_bounds, true);
  GetBounds(&client_bounds, false);
  int left_edge_width = client_bounds.left - window_bounds.left;
  int top_edge_height = client_bounds.top - window_bounds.top +
      location_bar_->height();
  int right_edge_width = window_bounds.right - client_bounds.right;
  int bottom_edge_height = window_bounds.bottom - client_bounds.bottom;

  // Now resize the window. This will result in Layout() getting called again
  // and the contents getting sized to the value specified in |contents_bounds|
  SetWindowPos(NULL, contents_bounds.x() - left_edge_width,
               contents_bounds.y() - top_edge_height,
               contents_bounds.width() + left_edge_width + right_edge_width,
               contents_bounds.height() + top_edge_height + bottom_edge_height,
               SWP_NOZORDER | SWP_NOACTIVATE);
}

LRESULT SimpleVistaFrame::OnNCHitTest(const CPoint& pt) {
  SetMsgHandled(false);
  return 0;
}

LRESULT SimpleVistaFrame::OnNCCalcSize(BOOL w_param, LPARAM l_param) {
  SetMsgHandled(false);
  return 0;
}

void SimpleVistaFrame::OnNCLButtonDown(UINT flags, const CPoint& pt) {
  if (flags == HTSYSMENU) {
    POINT p = {0, 0};
    ::ClientToScreen(*this, &p);
    browser_->RunSimpleFrameMenu(gfx::Point(p.x, p.y), *this);
    SetMsgHandled(true);
  } else {
    SetMsgHandled(false);
  }
}

void SimpleVistaFrame::StartThrobber() {
  if (!throbber_running_) {
    icon_manager_->SetUpdatesEnabled(false);
    throbber_running_ = true;
    throbber_frame_ = 0;
    InitThrobberIcons();
    ::SendMessage(*this, WM_SETICON, static_cast<WPARAM>(ICON_SMALL),
                  reinterpret_cast<LPARAM>(g_throbber_icons[throbber_frame_]));
  }
}

void SimpleVistaFrame::DisplayNextThrobberFrame() {
  throbber_frame_ = (throbber_frame_ + 1) % kThrobberIconCount;
  ::SendMessage(*this, WM_SETICON, static_cast<WPARAM>(ICON_SMALL),
                reinterpret_cast<LPARAM>(g_throbber_icons[throbber_frame_]));
}

bool SimpleVistaFrame::IsThrobberRunning() {
  return throbber_running_;
}

void SimpleVistaFrame::StopThrobber() {
  if (throbber_running_) {
    throbber_running_ = false;
    icon_manager_->SetUpdatesEnabled(true);
  }
}

void SimpleVistaFrame::ValidateThrobber() {
  if (!browser_)
    return;
  TabContents* tc = browser_->GetSelectedTabContents();
  if (IsThrobberRunning()) {
    if (!tc || !tc->is_loading())
      StopThrobber();
    else
      DisplayNextThrobberFrame();
  } else if (tc && tc->is_loading()) {
    StartThrobber();
  }
}

void SimpleVistaFrame::Layout() {
  VistaFrame::Layout();

  // This happens while executing Init().
  if (!location_bar_)
    return;

  if (browser_->ShouldDisplayURLField()) {
    TabContentsContainerView* container = GetTabContentsContainer();
    gfx::Size s = location_bar_->GetPreferredSize();
    location_bar_->SetBounds(container->x() - kLocationBarOutdent,
                             container->y() - kLocationBarOutdent,
                             container->width() + kLocationBarOutdent * 2,
                             s.height());
    container->SetBounds(container->x(),
                         location_bar_->y() + location_bar_->height() -
                         kLocationBarSpacing, container->width(),
                         container->height() - location_bar_->height() +
                         3);
    location_bar_->SetVisible(true);
    location_bar_->Layout();
  } else {
    location_bar_->SetVisible(false);
  }
}

void SimpleVistaFrame::InitAfterHWNDCreated() {
  icon_manager_.reset(new WebAppIconManager(*this));
  VistaFrame::InitAfterHWNDCreated();
}

TabContents* SimpleVistaFrame::GetTabContents() {
  return browser_->GetSelectedTabContents();
}

void SimpleVistaFrame::OnInputInProgress(bool in_progress) {
}

void SimpleVistaFrame::UpdateLocationBar() {
  if (location_bar_ && location_bar_->IsVisible())
    location_bar_->Update(NULL);
}

