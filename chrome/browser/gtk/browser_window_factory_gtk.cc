// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browser_window.h"

#include "chrome/browser/browser.h"
#include "chrome/browser/gtk/browser_window_gtk.h"
#include "chrome/browser/gtk/find_bar_gtk.h"

BrowserWindow* BrowserWindow::CreateBrowserWindow(Browser* browser) {
  return new BrowserWindowGtk(browser);
}

FindBar* BrowserWindow::CreateFindBar(Browser* browser) {
  return new FindBarGtk(browser);
}
