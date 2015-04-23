// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_PLUGINS_GTK_PLUGIN_CONTAINER_H_
#define WEBKIT_GLUE_PLUGINS_GTK_PLUGIN_CONTAINER_H_

// Windowed plugins are embedded via XEmbed, which is implemented by
// GtkPlug/GtkSocket.  But we want to control sizing and positioning
// directly, so we need a subclass of GtkSocket that sidesteps the
// size_request handler.
//
// The custom size_request handler just reports the size set by
// gtk_plugin_container_set_size.

typedef struct _GtkWidget GtkWidget;

// Return a new GtkPluginContainer.
// Intentionally GTK-style here since we're creating a custom GTK widget.
// This is a GtkSocket subclass; see its documentation for available methods.
GtkWidget* gtk_plugin_container_new();

// Sets the size of the GtkPluginContainer.
void gtk_plugin_container_set_size(GtkWidget *widget, int width, int height);

#endif  // WEBKIT_GLUE_PLUGINS_GTK_PLUGIN_CONTAINER_H_
