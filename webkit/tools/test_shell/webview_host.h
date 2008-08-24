// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_TOOLS_TEST_SHELL_WEBVIEW_HOST_H__
#define WEBKIT_TOOLS_TEST_SHELL_WEBVIEW_HOST_H__

#include <windows.h>

#include "base/gfx/rect.h"
#include "base/scoped_ptr.h"
#include "webkit/tools/test_shell/webwidget_host.h"

struct WebPreferences;
class WebView;
class WebViewDelegate;

// This class is a simple HWND-based host for a WebView
class WebViewHost : public WebWidgetHost {
 public:
  // The new instance is deleted once the associated HWND is destroyed.  The
  // newly created window should be resized after it is created, using the
  // MoveWindow (or equivalent) function.
  static WebViewHost* Create(HWND parent_window,
                             WebViewDelegate* delegate,
                             const WebPreferences& prefs);

  WebView* webview() const;

 protected:
  virtual bool WndProc(UINT message, WPARAM wparam, LPARAM lparam) {
    return false;
  }
};

#endif  // WEBKIT_TOOLS_TEST_SHELL_WEBVIEW_HOST_H__

