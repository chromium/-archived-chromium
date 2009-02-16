// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/debugger/tools_window.h"

#include "chrome/browser/debugger/tools_view.h"
#include "chrome/views/window.h"

ToolsWindow::ToolsWindow() : window_(NULL), tools_view_(NULL) {
}

ToolsWindow::~ToolsWindow() {
}

void ToolsWindow::Show(int inspected_process_id,
                       int inspected_view_id) {
  if (window_) {
    window_->Show();
    return;
  }

  tools_view_ = new ToolsView(inspected_process_id, inspected_view_id);
  window_ = views::Window::CreateChromeWindow(NULL, gfx::Rect(), this);
  window_->Show();
}

void ToolsWindow::SendToolsClientMessage(int tools_message_type,
                                         const std::wstring& body) {
  if (!tools_view_)
    return;
  tools_view_->SendToolsClientMessage(tools_message_type, body);
}

std::wstring ToolsWindow::GetWindowTitle() const {
  return L"Developer Tools";
}

void ToolsWindow::WindowClosing() {
  if (tools_view_) {
    tools_view_->OnWindowClosing();
    tools_view_ = NULL;
    window_ = NULL;
  }
}

bool ToolsWindow::CanResize() const {
  return true;
}

views::View* ToolsWindow::GetContentsView() {
  return tools_view_;
}
