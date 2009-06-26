// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_TOOLS_TEST_SHELL_WEBVIEW_HOST_H_
#define WEBKIT_TOOLS_TEST_SHELL_WEBVIEW_HOST_H_

#include <map>

#include "base/basictypes.h"
#include "base/gfx/native_widget_types.h"
#include "base/gfx/rect.h"
#include "webkit/tools/test_shell/webwidget_host.h"

struct WebPreferences;
class WebView;
class WebViewDelegate;

#if defined(OS_LINUX)
typedef struct _GtkSocket GtkSocket;
#endif

// This class is a simple NativeView-based host for a WebView
class WebViewHost : public WebWidgetHost {
 public:
  // The new instance is deleted once the associated NativeView is destroyed.
  // The newly created window should be resized after it is created, using the
  // MoveWindow (or equivalent) function.
  static WebViewHost* Create(gfx::NativeView parent_view,
                             WebViewDelegate* delegate,
                             const WebPreferences& prefs);

  WebView* webview() const;

#if defined(OS_LINUX)
  // Create a new plugin parent container, returning its X window id for
  // embedders to use.
  GdkNativeWindow CreatePluginContainer();

  // Map a GdkNativeWindow returned by CreatePluginContainer() back to
  // the GtkWidget hosting it.  Used when we get a message back from the
  // renderer indicating a plugin needs to move.
  GtkWidget* MapIDToWidget(GdkNativeWindow id);
#endif

 protected:
#if defined(OS_WIN)
  virtual bool WndProc(UINT message, WPARAM wparam, LPARAM lparam) {
    return false;
  }
#endif

#if defined(OS_LINUX)
  // A map used for MapIDToWidget() above.
  typedef std::map<GdkNativeWindow, GtkWidget*> NativeWindowToWidgetMap;
  NativeWindowToWidgetMap native_window_to_widget_map_;

  // Callback for when one of our plugins goes away.
  static gboolean OnPlugRemovedThunk(GtkSocket* socket,
                                     WebViewHost* web_view_host) {
    return web_view_host->OnPlugRemoved(socket);
  }
  gboolean OnPlugRemoved(GtkSocket* socket);
#endif
};

#endif  // WEBKIT_TOOLS_TEST_SHELL_WEBVIEW_HOST_H_
