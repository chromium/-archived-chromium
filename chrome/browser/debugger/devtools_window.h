// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEBUGGER_DEV_TOOLS_WINDOW_H_
#define CHROME_BROWSER_DEBUGGER_DEV_TOOLS_WINDOW_H_

#include <string>

#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "chrome/browser/debugger/devtools_client_host.h"
#include "chrome/browser/tabs/tab_strip_model.h"

namespace IPC {
class Message;
}

class Browser;
class Profile;
class RenderViewHost;
class TabContents;

class DevToolsWindow : public DevToolsClientHost,
                              TabStripModelObserver {
 public:
  // Factory method for creating platform specific devtools windows.
  DevToolsWindow(Profile* profile);
  virtual ~DevToolsWindow();

  // Show this window.
  void Show();

  RenderViewHost* GetRenderViewHost() const;

  // DevToolsClientHost override.
  virtual DevToolsWindow* AsDevToolsWindow();
  virtual void InspectedTabClosing();
  virtual void SetInspectedTabUrl(const std::string& url);
  virtual void SendMessageToClient(const IPC::Message& message);

  // TabStripModelObserver implementation
  virtual void TabClosingAt(TabContents* contents, int index);

 private:
  scoped_ptr<Browser> browser_;
  TabContents* tab_contents_;
  std::string inspected_url_;
  bool inspected_tab_closing_;
  DISALLOW_COPY_AND_ASSIGN(DevToolsWindow);
};

#endif  // CHROME_BROWSER_DEBUGGER_DEV_TOOLS_WINDOW_H_
