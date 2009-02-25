// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/frame/opaque_frame.h"

#include "chrome/browser/browser_list.h"
#include "chrome/browser/views/frame/browser_view.h"
#include "chrome/browser/views/frame/opaque_non_client_view.h"
#include "chrome/browser/views/tabs/tab_strip.h"
#include "chrome/views/window_delegate.h"

///////////////////////////////////////////////////////////////////////////////
// OpaqueFrame, public:

OpaqueFrame::OpaqueFrame(BrowserView* browser_view)
    : CustomFrameWindow(browser_view, new OpaqueNonClientView(this,
                                                              browser_view)),
      browser_view_(browser_view) {
  browser_view_->set_frame(this);
}

OpaqueFrame::~OpaqueFrame() {
}

void OpaqueFrame::Init() {
  CustomFrameWindow::Init(NULL, gfx::Rect());
}

///////////////////////////////////////////////////////////////////////////////
// OpaqueFrame, BrowserFrame implementation:

gfx::Rect OpaqueFrame::GetWindowBoundsForClientBounds(
    const gfx::Rect& client_bounds) {
  return GetOpaqueNonClientView()->GetWindowBoundsForClientBounds(
      client_bounds);
}

void OpaqueFrame::SizeToContents(const gfx::Rect& contents_bounds) {
  gfx::Rect window_bounds = GetOpaqueNonClientView()->
      GetWindowBoundsForClientBounds(contents_bounds);
  SetBounds(window_bounds);
}

gfx::Rect OpaqueFrame::GetBoundsForTabStrip(TabStrip* tabstrip) const {
  return GetOpaqueNonClientView()->GetBoundsForTabStrip(tabstrip);
}

void OpaqueFrame::UpdateThrobber(bool running) {
  // TODO(beng): pass |running| through rather than letting
  //             OpaqueNonClientView's TabIconView try and figure it out.
  // The throbber doesn't run in the Windows TaskBar, so we just update the
  // non-client view. Updating the taskbar is muy expensivo.
  GetOpaqueNonClientView()->UpdateWindowIcon();
}

views::Window* OpaqueFrame::GetWindow() {
  return this;
}

const views::Window* OpaqueFrame::GetWindow() const {
  return this;
}

///////////////////////////////////////////////////////////////////////////////
// OpaqueFrame, views::CustomFrameWindow overrides:

void OpaqueFrame::UpdateWindowIcon() {
  CustomFrameWindow::UpdateWindowIcon();
  GetOpaqueNonClientView()->UpdateWindowIcon();
}

int OpaqueFrame::GetShowState() const {
  return browser_view_->GetShowState();
}

///////////////////////////////////////////////////////////////////////////////
// OpaqueFrame, views::WidgetWin overrides:

bool OpaqueFrame::AcceleratorPressed(views::Accelerator* accelerator) {
  return browser_view_->AcceleratorPressed(*accelerator);
}

bool OpaqueFrame::GetAccelerator(int cmd_id, views::Accelerator* accelerator) {
  return browser_view_->GetAccelerator(cmd_id, accelerator);
}

void OpaqueFrame::OnEndSession(BOOL ending, UINT logoff) {
  BrowserList::WindowsSessionEnding();
}

void OpaqueFrame::OnEnterSizeMove() {
  browser_view_->WindowMoveOrResizeStarted();
}

void OpaqueFrame::OnInitMenuPopup(HMENU menu, UINT position,
                                  BOOL is_system_menu) {
  browser_view_->PrepareToRunSystemMenu(menu);
}

LRESULT OpaqueFrame::OnMouseActivate(HWND window, UINT hittest_code,
                                     UINT message) {
  return browser_view_->ActivateAppModalDialog() ? MA_NOACTIVATEANDEAT
                                                 : MA_ACTIVATE;
}

void OpaqueFrame::OnMove(const CPoint& point) {
  browser_view_->WindowMoved();
}

void OpaqueFrame::OnMoving(UINT param, const RECT* new_bounds) {
  browser_view_->WindowMoved();
}

LRESULT OpaqueFrame::OnNCActivate(BOOL active) {
  if (browser_view_->ActivateAppModalDialog())
    return TRUE;

  CustomFrameWindow::OnNCActivate(active);
  browser_view_->ActivationChanged(!!active);
  return TRUE;
}

void OpaqueFrame::OnSysCommand(UINT notification_code, CPoint click) {
  if (!browser_view_->SystemCommandReceived(notification_code,
                                            gfx::Point(click))) {
    // Use the default implementation for any other command.
    CustomFrameWindow::OnSysCommand(notification_code, click);
  }
}

///////////////////////////////////////////////////////////////////////////////
// OpaqueFrame, private:

OpaqueNonClientView* OpaqueFrame::GetOpaqueNonClientView() const {
  // We can safely assume that this conversion is true.
  return static_cast<OpaqueNonClientView*>(non_client_view_);
}

