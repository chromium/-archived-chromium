// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/debugger/devtools_window_win.h"

#include "chrome/browser/debugger/devtools_manager.h"
#include "chrome/browser/debugger/devtools_view.h"
#include "chrome/views/window.h"


// static
DevToolsWindow* DevToolsWindow::Create(DevToolsInstanceDescriptor* descriptor) {
  DevToolsView* view = new DevToolsView(descriptor);
  DevToolsWindowWin* window = new DevToolsWindowWin(view);
  descriptor->SetDevToolsWindow(window);
  views::Window::CreateChromeWindow(NULL, gfx::Rect(), window);
  return window;
}

DevToolsWindowWin::DevToolsWindowWin(DevToolsView* view)
    : tools_view_(view) {
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

void DevToolsWindowWin::Close() {
  if (window()) {
    window()->Close();
  } else {
    NOTREACHED();
  }
}

std::wstring DevToolsWindowWin::GetWindowTitle() const {
  return L"Developer Tools";
}

void DevToolsWindowWin::WindowClosing() {
  if (tools_view_) {
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
