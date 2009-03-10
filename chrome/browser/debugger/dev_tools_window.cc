// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/debugger/dev_tools_window.h"

#include "chrome/browser/debugger/dev_tools_view.h"
#include "chrome/views/window.h"

DevToolsWindow::DevToolsWindow() : window_(NULL), tools_view_(NULL) {
}

DevToolsWindow::~DevToolsWindow() {
}

void DevToolsWindow::Show(int inspected_process_id,
                       int inspected_view_id) {
  if (window_) {
    window_->Show();
    return;
  }

  tools_view_ = new DevToolsView(inspected_process_id, inspected_view_id);
  window_ = views::Window::CreateChromeWindow(NULL, gfx::Rect(), this);
  window_->Show();
}

void DevToolsWindow::SendDevToolsClientMessage(const IPC::Message& message) {
  if (!tools_view_)
    return;
  tools_view_->SendDevToolsClientMessage(message);
}

std::wstring DevToolsWindow::GetWindowTitle() const {
  return L"Developer Tools";
}

void DevToolsWindow::WindowClosing() {
  if (tools_view_) {
    tools_view_->OnWindowClosing();
    tools_view_ = NULL;
    window_ = NULL;
  }
}

bool DevToolsWindow::CanResize() const {
  return true;
}

views::View* DevToolsWindow::GetContentsView() {
  return tools_view_;
}
