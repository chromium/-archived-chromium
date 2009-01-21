// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browser_window.h"
#include "chrome/browser/browser_window_controller.h"

// Create the controller for the Browser, which handles loading the browser 
// window from the nib. The controller takes ownership of |browser|.
// static
BrowserWindow* BrowserWindow::CreateBrowserWindow(Browser* browser) {
  // TODO(pinkerton): figure out ownership model. If BrowserList keeps track
  // of the browser windows, it will probably tell us when it needs to go
  // away, and it seems we need to feed back to that when we get a 
  // performClose: from the UI.
  BrowserWindowController* controller = 
      [[BrowserWindowController alloc] initWithBrowser:browser];
  return [controller browserWindow];
}

