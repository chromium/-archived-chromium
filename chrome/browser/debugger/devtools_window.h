// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEBUGGER_DEV_TOOLS_WINDOW_H_
#define CHROME_BROWSER_DEBUGGER_DEV_TOOLS_WINDOW_H_

#include "base/basictypes.h"
#include "chrome/browser/debugger/devtools_client_host.h"

class RenderViewHost;

class DevToolsWindow : public DevToolsClientHost {
 public:
  // Factory method for creating platform specific devtools windows.
  static DevToolsWindow* Create();

  virtual ~DevToolsWindow() {}

  // Show this window.
  virtual void Show() = 0;
  virtual bool HasRenderViewHost(const RenderViewHost& rvh) const = 0;

  // DevToolsClientHost override.
  virtual DevToolsWindow* AsDevToolsWindow() { return this; }

 protected:
  DevToolsWindow() : DevToolsClientHost() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(DevToolsWindow);
};

#endif  // CHROME_BROWSER_DEBUGGER_DEV_TOOLS_WINDOW_H_
