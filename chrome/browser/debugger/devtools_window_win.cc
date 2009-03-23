// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/debugger/devtools_window_win.h"

#include "chrome/browser/debugger/devtools_manager.h"
#include "chrome/browser/debugger/devtools_view.h"
#include "chrome/browser/debugger/devtools_window.h"
#include "chrome/views/window/window.h"


// static
DevToolsWindow* DevToolsWindow::Create() {
  DevToolsView* view = new DevToolsView();
  DevToolsWindowWin* window = new DevToolsWindowWin(view);
  views::Window::CreateChromeWindow(NULL, gfx::Rect(), window);
  return window;
}

DevToolsWindowWin::DevToolsWindowWin(DevToolsView* view)
    : DevToolsWindow(),
      tools_view_(view) {
}

DevToolsWindowWin::~DevToolsWindowWin() {
  DCHECK(!tools_view_);
}

void DevToolsWindowWin::Show() {
  if (window()) {
    window()->Show();
  } else {
    NOTREACHED();
  }
}

bool DevToolsWindowWin::HasRenderViewHost(const RenderViewHost& rvh) const {
  if (tools_view_) {
    return tools_view_->HasRenderViewHost(rvh);
  }
  return false;
}

void DevToolsWindowWin::InspectedTabClosing() {
  if (window()) {
    window()->Close();
  } else {
    NOTREACHED();
  }
}

void DevToolsWindowWin::SendMessageToClient(const IPC::Message& message) {
  if (tools_view_) {
    tools_view_->SendMessageToClient(message);
  }
}

std::wstring DevToolsWindowWin::GetWindowTitle() const {
  return L"Developer Tools";
}

void DevToolsWindowWin::WindowClosing() {
  if (tools_view_) {
    NotifyCloseListener();

    ReleaseWindow();
    tools_view_->OnWindowClosing();
    tools_view_ = NULL;
  } else {
    NOTREACHED() << "WindowClosing called twice.";
  }
}

bool DevToolsWindowWin::CanResize() const {
  return true;
}

views::View* DevToolsWindowWin::GetContentsView() {
  return tools_view_;
}

void DevToolsWindowWin::DeleteDelegate() {
  DCHECK(!tools_view_) << "WindowClosing should have been called.";
  delete this;
}
