// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_VIEWS_FOCUS_FOCUS_UTIL_WIN_H_
#define CHROME_VIEWS_FOCUS_FOCUS_UTIL_WIN_H_

#include <windows.h>

namespace views {

// Marks the passed |hwnd| as supporting mouse-wheel message rerouting.
// We reroute the mouse wheel messages to such HWND when they are under the
// mouse pointer (but are not the active window)
void SetWindowSupportsRerouteMouseWheel(HWND hwnd);

// Forwards mouse wheel messages to the window under it.
// Windows sends mouse wheel messages to the currently active window.
// This causes a window to scroll even if it is not currently under the mouse
// wheel. The following code gives mouse wheel messages to the window under the
// mouse wheel in order to scroll that window. This is arguably a better user
// experience.  The returns value says whether the mouse wheel message was
// successfully redirected.
bool RerouteMouseWheel(HWND window, WPARAM w_param, LPARAM l_param);

}   // namespace views

#endif  // CHROME_VIEWS_FOCUS_FOCUS_UTIL_WIN_H_
