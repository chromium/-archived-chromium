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
  g_object_set_data(G_OBJECT(host->view_), "webwidgethost", host);

  host->webwidget_ = WebView::Create(delegate, prefs);
  host->webwidget_->Layout();

  return host;
}

WebView* WebViewHost::webview() const {
  return static_cast<WebView*>(webwidget_);
}

GdkNativeWindow WebViewHost::CreatePluginContainer() {
  GtkWidget* plugin_container = gtk_plugin_container_new();
  g_signal_connect(G_OBJECT(plugin_container), "plug-removed",
                   G_CALLBACK(OnPlugRemovedThunk), this);
  gtk_container_add(GTK_CONTAINER(view_handle()), plugin_container);
  gtk_widget_show(plugin_container);
  gtk_widget_realize(plugin_container);

  GdkNativeWindow id = gtk_socket_get_id(GTK_SOCKET(plugin_container));

  native_window_to_widget_map_.insert(std::make_pair(id, plugin_container));

  return id;
}

GtkWidget* WebViewHost::MapIDToWidget(GdkNativeWindow id) {
  NativeWindowToWidgetMap::const_iterator i =
      native_window_to_widget_map_.find(id);
  if (i != native_window_to_widget_map_.end())
    return i->second;

  LOG(ERROR) << "Request for widget host for unknown window id " << id;

  return NULL;
}

void WebViewHost::OnPluginWindowDestroyed(GdkNativeWindow id) {
  GtkWidget* plugin_container = MapIDToWidget(id);
  if (!plugin_container)
    return;

  native_window_to_widget_map_.erase(id);
  gtk_widget_destroy(plugin_container);
}

gboolean WebViewHost::OnPlugRemoved(GtkSocket* socket) {
  return TRUE;  // Don't destroy our widget; we manage it ourselves.
}

