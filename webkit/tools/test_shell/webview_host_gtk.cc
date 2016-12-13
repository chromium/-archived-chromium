// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gtk/gtk.h>

#include "webkit/tools/test_shell/webview_host.h"

#include "base/logging.h"
#include "base/gfx/rect.h"
#include "base/gfx/size.h"
#include "skia/ext/platform_canvas.h"
#include "webkit/glue/plugins/gtk_plugin_container.h"
#include "webkit/glue/webview.h"

// static
WebViewHost* WebViewHost::Create(GtkWidget* parent_view,
                                 WebViewDelegate* delegate,
                                 const WebPreferences& prefs) {
  WebViewHost* host = new WebViewHost();

  host->view_ = WebWidgetHost::CreateWidget(parent_view, host);
  host->plugin_container_manager_.set_host_widget(host->view_);
  g_object_set_data(G_OBJECT(host->view_), "webwidgethost", host);

  host->webwidget_ = WebView::Create(delegate, prefs);
  host->webwidget_->Layout();

  return host;
}

WebView* WebViewHost::webview() const {
  return static_cast<WebView*>(webwidget_);
}

GdkNativeWindow WebViewHost::CreatePluginContainer() {
  return plugin_container_manager_.CreatePluginContainer();
}

void WebViewHost::OnPluginWindowDestroyed(GdkNativeWindow id) {
  plugin_container_manager_.DestroyPluginContainer(id);
}
