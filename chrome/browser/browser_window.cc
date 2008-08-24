// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browser_window.h"

#include <windows.h>

void BrowserWindow::InfoBubbleClosing() {
  // TODO(beng): (Cleanup) - move out of here!
  HWND hwnd = static_cast<HWND>(GetPlatformID());
  // The frame is really inactive, send notification now.
  DefWindowProc(hwnd, WM_NCACTIVATE, FALSE, 0);
}
