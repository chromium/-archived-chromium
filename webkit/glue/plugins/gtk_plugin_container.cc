// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/glue/plugins/gtk_plugin_container.h"

#include <gtk/gtk.h>

#include "base/basictypes.h"

namespace {

// NOTE: This class doesn't have constructors/destructors, it is created
// through GLib's object management.
class GtkPluginContainer : public GtkSocket {
 public:
  static GtkWidget* CreateNewWidget() {
    GtkWidget* container = GTK_WIDGET(g_object_new(GetType(), NULL));
    g_signal_connect(GTK_SOCKET(container), "plug-removed",
                     G_CALLBACK(OnPlugRemoved), NULL);
    return container;
  }

  // Sets the requested size of the widget.
  void set_size(int width, int height) {
    width_ = width;
    height_ = height;
  }

  // Casts a widget into a GtkPluginContainer, after checking the type.
  template <class T>
  static GtkPluginContainer *CastChecked(T *instance) {
    return G_TYPE_CHECK_INSTANCE_CAST(instance, GetType(), GtkPluginContainer);
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
        0, &InstanceInit,
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

  // Implementation of the instance initializer (constructor).
  static void InstanceInit(GTypeInstance *instance, gpointer klass) {
    GtkPluginContainer *container = CastChecked(instance);
    container->set_size(0, 0);
  }

  // Report our allocation size during size requisition.
  static void HandleSizeRequest(GtkWidget* widget,
                                GtkRequisition* requisition) {
    GtkPluginContainer *container = CastChecked(widget);
    requisition->width = container->width_;
    requisition->height = container->height_;
  }

  static gboolean OnPlugRemoved(GtkSocket* socket) {
    // This is called when the other side of the socket goes away.
    // We return TRUE to indicate that we don't want to destroy our side.
    return TRUE;
  }

  int width_;
  int height_;
  DISALLOW_IMPLICIT_CONSTRUCTORS(GtkPluginContainer);
};

}  // anonymous namespace

// Create a new instance of our GTK widget object.
GtkWidget* gtk_plugin_container_new() {
  return GtkPluginContainer::CreateNewWidget();
}

void gtk_plugin_container_set_size(GtkWidget *widget, int width, int height) {
  GtkPluginContainer::CastChecked(widget)->set_size(width, height);
  // Signal the parent that the size request has changed.
  gtk_widget_queue_resize_no_redraw(widget);
}
