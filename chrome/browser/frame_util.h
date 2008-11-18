// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FRAME_UTIL_H_
#define CHROME_BROWSER_FRAME_UTIL_H_

#include <windows.h>

class BrowserWindow;

// Static helpers for frames. Basically shared code until Magic Browzr lands.
class FrameUtil {
 public:
  // Return a ChromeFrame instance given an hwnd.
  static BrowserWindow* GetBrowserWindowForHWND(HWND hwnd);

  // Invoked when windows is shutting down (or the user is logging off). When
  // this method returns windows is going to kill our process. As such, this
  // blocks until the shut-down has been marked as clean.
  static void EndSession();
};

#endif  // #ifndef CHROME_BROWSER_FRAME_UTIL_H__

