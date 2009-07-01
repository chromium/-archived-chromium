// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEBUGGER_DEV_TOOLS_WINDOW_H_
#define CHROME_BROWSER_DEBUGGER_DEV_TOOLS_WINDOW_H_

#include <string>

#include "base/basictypes.h"
#include "chrome/browser/debugger/devtools_client_host.h"
#include "chrome/common/notification_registrar.h"
#include "chrome/common/notification_service.h"

namespace IPC {
class Message;
}

class Browser;
class BrowserWindow;
class Profile;
class RenderViewHost;
class TabContents;

class DevToolsWindow : public DevToolsClientHost, public NotificationObserver {
 public:
  static DevToolsWindow* CreateDevToolsWindow(Profile* profile,
                                              RenderViewHost* inspected_rvh,
                                              bool docked);

  static TabContents* GetDevToolsContents(TabContents* inspected_tab);

  virtual ~DevToolsWindow();
  virtual void Show() = 0;
  bool is_docked() { return docked_; };
  RenderViewHost* GetRenderViewHost();

  // DevToolsClientHost override.
  virtual DevToolsWindow* AsDevToolsWindow();
  virtual void SendMessageToClient(const IPC::Message& message);

  // NotificationObserver override.
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  TabContents* tab_contents() { return tab_contents_; }
  Browser* browser() { return browser_; }

 protected:
  DevToolsWindow(bool docked);
  GURL GetContentsUrl();
  void InitTabContents(TabContents* tab_contents);

  TabContents* tab_contents_;
  Browser* browser_;

 private:
  static BrowserWindow* GetBrowserWindow(RenderViewHost* rvh);
  NotificationRegistrar registrar_;

  bool docked_;
  DISALLOW_COPY_AND_ASSIGN(DevToolsWindow);
};

#endif  // CHROME_BROWSER_DEBUGGER_DEV_TOOLS_WINDOW_H_
