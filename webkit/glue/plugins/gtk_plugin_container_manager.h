// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_PLUGINS_GTK_PLUGIN_CONTAINER_MANAGER_H_
#define WEBKIT_GLUE_PLUGINS_GTK_PLUGIN_CONTAINER_MANAGER_H_

#include <gtk/gtk.h>
#include <map>
#include "base/gfx/native_widget_types.h"

typedef struct _GtkWidget GtkWidget;
struct WebPluginGeometry;

// Helper class that creates and manages plugin containers (GtkSocket).
class GtkPluginContainerManager {
 public:
  GtkPluginContainerManager() : host_widget_(NULL) { }

  // Sets the widget that will host the plugin containers. Must be a GtkFixed.
  void set_host_widget(GtkWidget *widget) { host_widget_ = widget; }

  // Creates a new plugin container, returning its XID.
  gfx::PluginWindowHandle CreatePluginContainer();

  // Destroys a plugin container, given its XID.
  void DestroyPluginContainer(gfx::PluginWindowHandle container);

  // Takes an update from WebKit about a plugin's position and side and moves
  // the plugin accordingly.
  void MovePluginContainer(const WebPluginGeometry& move);

 private:
  // Maps a plugin container XID to the corresponding widget.
  GtkWidget* MapIDToWidget(gfx::PluginWindowHandle id);

  // Callback for when the plugin container loses its XID, so that it can be
  // removed from plugin_window_to_widget_map_.
  static gboolean UnrealizeCallback(GtkWidget *widget, void *user_data);

  // Parent of the plugin containers.
  GtkWidget* host_widget_;

  // A map that associates plugin containers to their XID.
  typedef std::map<gfx::PluginWindowHandle, GtkWidget*> PluginWindowToWidgetMap;
  PluginWindowToWidgetMap plugin_window_to_widget_map_;
};

#endif  // WEBKIT_GLUE_PLUGINS_GTK_PLUGIN_CONTAINER_MANAGER_H_
