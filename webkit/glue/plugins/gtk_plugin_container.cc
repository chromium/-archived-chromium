// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/glue/plugins/gtk_plugin_container.h"

#include <gtk/gtk.h>

namespace {

// This class has no members/methods, and is only used for namespacing purposes.
class GtkPluginContainer {
 public:
  static GtkWidget* CreateNewWidget() {
    GtkWidget* container = GTK_WIDGET(g_object_new(GetType(), NULL));
    g_signal_connect(GTK_SOCKET(container), "plug-removed",
                     G_CALLBACK(OnPlugRemoved), NULL);
    return container;
  }

 private:
  // Create and register our custom container type with GTK.
  static GType GetType() {
    static GType type = 0;  // We only want to register our type once.
    if (!type) {
      static const GTypeInfo info = {
        sizeof(GtkSocketClass),
        NULL, NULL,
        static_cast<GClassInitFunc>(&ClassInit),
        NULL, NULL,
        sizeof(GtkSocket),  // We are identical to a GtkSocket.
        0, NULL,
      };
      type = g_type_register_static(GTK_TYPE_SOCKET,
                                    "GtkPluginContainer",
                                    &info,
                                    static_cast<GTypeFlags>(0));
    }
    return type;
  }

  // Implementation of the class initializer.
  static void ClassInit(gpointer klass, gpointer class_data_unusued) {
    GtkWidgetClass* widget_class = reinterpret_cast<GtkWidgetClass*>(klass);
    widget_class->size_request = &HandleSizeRequest;
  }

  // Report our allocation size during size requisition.
  static void HandleSizeRequest(GtkWidget* widget,
                                GtkRequisition* requisition) {
    requisition->width = widget->allocation.width;
    requisition->height = widget->allocation.height;
  }

  static gboolean OnPlugRemoved(GtkSocket* socket) {
    // This is called when the other side of the socket goes away.
    // We return TRUE to indicate that we don't want to destroy our side.
    return TRUE;
  }
};

}  // anonymous namespace

// Create a new instance of our GTK widget object.
GtkWidget* gtk_plugin_container_new() {
  return GtkPluginContainer::CreateNewWidget();
}
