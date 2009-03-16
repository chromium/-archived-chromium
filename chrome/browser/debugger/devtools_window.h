// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEBUGGER_DEV_TOOLS_WINDOW_H_
#define CHROME_BROWSER_DEBUGGER_DEV_TOOLS_WINDOW_H_

#include "base/basictypes.h"

class DevToolsInstanceDescriptor;

class DevToolsWindow {
 public:
  static DevToolsWindow* Create(DevToolsInstanceDescriptor* descriptor);
  virtual ~DevToolsWindow() {}

  // Show developer tools window.
  virtual void Show() = 0;
  virtual void Close() = 0;

 protected:
  DevToolsWindow() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(DevToolsWindow);
};

#endif  // CHROME_BROWSER_DEBUGGER_DEV_TOOLS_WINDOW_H_
