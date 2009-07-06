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
#if defined(OS_LINUX)
#include "webkit/glue/plugins/gtk_plugin_container_manager.h"
#endif

struct WebPreferences;
class WebView;
class WebViewDelegate;

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

  // Called when a plugin has been destroyed.  Lets us clean up our side.
  void OnPluginWindowDestroyed(GdkNativeWindow id);

  GtkPluginContainerManager* plugin_container_manager() {
    return &plugin_container_manager_;
  }
#endif

 protected:
#if defined(OS_WIN)
  virtual bool WndProc(UINT message, WPARAM wparam, LPARAM lparam) {
    return false;
  }
#endif

#if defined(OS_LINUX)
  // Helper class that creates and moves plugin containers.
  GtkPluginContainerManager plugin_container_manager_;
#endif
};

#endif  // WEBKIT_TOOLS_TEST_SHELL_WEBVIEW_HOST_H_
