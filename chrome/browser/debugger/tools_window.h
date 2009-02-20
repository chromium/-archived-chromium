// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEBUGGER_TOOLS_WINDOW_H_
#define CHROME_BROWSER_DEBUGGER_TOOLS_WINDOW_H_

#include "base/basictypes.h"

#include "chrome/views/window_delegate.h"

namespace views {
class Window;
}
class ToolsView;
class TabContents;

class ToolsWindow : public views::WindowDelegate {
 public:
  ToolsWindow();
  virtual ~ToolsWindow();

  // Show inspector window for the tab
  void Show(int inspected_process_id,
            int inspected_view_id);

  void SendToolsClientMessage(int tools_message_type,
                              const std::wstring& body);

 private:
  // views::WindowDelegate methods:
  virtual std::wstring GetWindowTitle() const;
  virtual void WindowClosing();
  virtual bool CanResize() const;
  virtual views::View* GetContentsView();

  views::Window* window_;
  ToolsView* tools_view_;

  DISALLOW_COPY_AND_ASSIGN(ToolsWindow);
};

#endif  // CHROME_BROWSER_DEBUGGER_TOOLS_WINDOW_H_

