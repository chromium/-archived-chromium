// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEBUGGER_DEVTOOLS_WINDOW_WIN_H_
#define CHROME_BROWSER_DEBUGGER_DEVTOOLS_WINDOW_WIN_H_

#include "base/basictypes.h"
#include "chrome/browser/debugger/devtools_window.h"
#include "chrome/views/window_delegate.h"

namespace views {
class Window;
}
class DevToolsInstanceDescriptor;
class DevToolsView;
class TabContents;

class DevToolsWindowWin : public DevToolsWindow,
                          public views::WindowDelegate {
 public:
  virtual ~DevToolsWindowWin();

  // Show developer tools window.
  virtual void Show();
  virtual void Close();

 private:
  friend class DevToolsWindow;
  explicit DevToolsWindowWin(DevToolsView* view);

  // views::WindowDelegate methods:
  virtual std::wstring GetWindowTitle() const;
  virtual void WindowClosing();
  virtual bool CanResize() const;
  virtual views::View* GetContentsView();
  virtual void DeleteDelegate();

  DevToolsView* tools_view_;

  DISALLOW_COPY_AND_ASSIGN(DevToolsWindowWin);
};

#endif  // CHROME_BROWSER_DEBUGGER_DEVTOOLS_WINDOW_WIN_H_
