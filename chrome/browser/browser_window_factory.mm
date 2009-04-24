// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/browser_window.h"
#include "chrome/browser/cocoa/browser_window_cocoa.h"
#include "chrome/browser/cocoa/browser_window_controller.h"
#include "chrome/browser/cocoa/find_bar_bridge.h"

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

// static
FindBar* BrowserWindow::CreateFindBar(Browser* browser) {
  // We could push the AddFindBar() call into the FindBarBridge
  // constructor or the FindBarCocoaController init, but that makes
  // unit testing difficult, since we would also require a
  // BrowserWindow object.
  BrowserWindowCocoa* window =
      static_cast<BrowserWindowCocoa*>(browser->window());
  FindBarBridge* bridge = new FindBarBridge();
  window->AddFindBar(bridge->find_bar_cocoa_controller());
  return bridge;
}
