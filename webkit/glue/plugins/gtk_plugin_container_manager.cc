// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/glue/plugins/gtk_plugin_container_manager.h"

#include <gtk/gtk.h>

#include "base/gfx/gtk_util.h"
#include "base/logging.h"
#include "webkit/glue/plugins/gtk_plugin_container.h"
#include "webkit/glue/webplugin.h"

// Helper function that always returns true. Used to prevent Gtk from
// destroying our socket when the plug goes away: we manage it ourselves.
static gboolean AlwaysTrue(void* unused) {
  return TRUE;
}

gfx::PluginWindowHandle GtkPluginContainerManager::CreatePluginContainer() {
  DCHECK(host_widget_);
  // If the current view hasn't been attached to a top-level window (e.g. it is
  // loaded in a background tab), it can't be realized without asserting in
  // Gtk, so we can't get the XID for the socket. Instead, don't create one.
  // We'll never see the plugin but it's better than crashing.
  // TODO(piman@google.com): figure out how to add the background tab to the
  // widget hierarchy, so that it can be realized. It doesn't have to be
  // visible.
  if (!gtk_widget_get_ancestor(host_widget_, GTK_TYPE_WINDOW)) {
    NOTIMPLEMENTED() << "Can't create plugins in background tabs.";
    return 0;
  }

  GtkWidget* plugin_container = gtk_plugin_container_new();
  g_signal_connect(G_OBJECT(plugin_container), "plug-removed",
                   G_CALLBACK(AlwaysTrue), NULL);
  // Add a connection to the "unrealize" signal so that if the parent widget
  // gets destroyed before the DestroyPluginContainer gets called, bad things
  // don't happen.
  g_signal_connect(G_OBJECT(plugin_container), "unrealize",
                   G_CALLBACK(UnrealizeCallback), this);
  gtk_container_add(GTK_CONTAINER(host_widget_), plugin_container);
  gtk_widget_show(plugin_container);
  gtk_widget_realize(plugin_container);

  gfx::PluginWindowHandle id = gtk_socket_get_id(GTK_SOCKET(plugin_container));

  plugin_window_to_widget_map_.insert(std::make_pair(id, plugin_container));

  return id;
}

void GtkPluginContainerManager::DestroyPluginContainer(
    gfx::PluginWindowHandle container) {
  GtkWidget* plugin_container = MapIDToWidget(container);
  if (!plugin_container)
    return;

  // This will call the UnrealizeCallback that will remove plugin_container
  // from the map.
  gtk_widget_destroy(plugin_container);
}

void GtkPluginContainerManager::MovePluginContainer(
    const WebPluginGeometry& move) {
  DCHECK(host_widget_);
  GtkWidget *widget = MapIDToWidget(move.window);
  if (!widget)
    return;

  DCHECK(!GTK_WIDGET_NO_WINDOW(widget));
  DCHECK(GTK_WIDGET_REALIZED(widget));

  if (!move.visible) {
    gtk_widget_hide(widget);
    return;
  } else {
    gtk_widget_show(widget);
  }

  GdkRectangle clip_rect = move.clip_rect.ToGdkRectangle();
  GdkRegion* clip_region = gdk_region_rectangle(&clip_rect);
  gfx::SubtractRectanglesFromRegion(clip_region, move.cutout_rects);
  gdk_window_shape_combine_region(widget->window, clip_region, 0, 0);
  gdk_region_destroy(clip_region);

  // Update the window position.  Resizing is handled by WebPluginDelegate.
  // TODO(deanm): Verify that we only need to move and not resize.
  // TODO(evanm): we should cache the last shape and position and skip all
  // of this business in the common case where nothing has changed.
  int current_x, current_y;

  // Until the above TODO is resolved, we can grab the last position
  // off of the GtkFixed with a bit of hackery.
  GValue value = {0};
  g_value_init(&value, G_TYPE_INT);
  gtk_container_child_get_property(GTK_CONTAINER(host_widget_), widget,
                                   "x", &value);
  current_x = g_value_get_int(&value);
  gtk_container_child_get_property(GTK_CONTAINER(host_widget_), widget,
                                   "y", &value);
  current_y = g_value_get_int(&value);
  g_value_unset(&value);

  if (move.window_rect.x() != current_x ||
      move.window_rect.y() != current_y) {
    // Calling gtk_fixed_move unnecessarily is a no-no, as it causes the
    // parent window to repaint!
    gtk_fixed_move(GTK_FIXED(host_widget_),
                   widget,
                   move.window_rect.x(),
                   move.window_rect.y());
  }

  gtk_plugin_container_set_size(widget,
                                move.window_rect.width(),
                                move.window_rect.height());
}

GtkWidget* GtkPluginContainerManager::MapIDToWidget(gfx::PluginWindowHandle id) {
  PluginWindowToWidgetMap::const_iterator i =
      plugin_window_to_widget_map_.find(id);
  if (i != plugin_window_to_widget_map_.end())
    return i->second;

  LOG(ERROR) << "Request for widget host for unknown window id " << id;

  return NULL;
}

void GtkPluginContainerManager::UnrealizeCallback(GtkWidget* widget,
                                                  void* user_data) {
  // This is the last chance to get the XID for the widget. Remove it from the
  // map here.
  GtkPluginContainerManager* plugin_container_manager =
      static_cast<GtkPluginContainerManager*>(user_data);
  gfx::PluginWindowHandle id = gtk_socket_get_id(GTK_SOCKET(widget));
  plugin_container_manager->plugin_window_to_widget_map_.erase(id);
}
