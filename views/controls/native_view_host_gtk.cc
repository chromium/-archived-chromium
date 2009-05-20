// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/controls/native_view_host_gtk.h"

#include <gtk/gtk.h>

#include "base/logging.h"
#include "views/widget/widget_gtk.h"

namespace views {

NativeViewHostGtk::NativeViewHostGtk() : destroy_signal_id_(0) {
}

NativeViewHostGtk::~NativeViewHostGtk() {
}

// static
View* NativeViewHostGtk::GetViewForNative(GtkWidget* widget) {
  gpointer user_data = g_object_get_data(G_OBJECT(widget), "chrome-view");
  return static_cast<View*>(user_data);
}

// static
void NativeViewHostGtk::SetViewForNative(GtkWidget* widget, View* view) {
  g_object_set_data(G_OBJECT(widget), "chrome-view", view);
}

void NativeViewHostGtk::Attach(GtkWidget* widget) {
  DCHECK(native_view() == NULL);
  DCHECK(widget);

  // Adds a mapping between the GtkWidget and us.
  SetViewForNative(widget, this);

  destroy_signal_id_ = g_signal_connect(G_OBJECT(widget), "destroy",
                                        G_CALLBACK(CallDestroy), NULL);

  set_native_view(widget);

  // First hide the new window. We don't want anything to draw (like sub-hwnd
  // borders), when we change the parent below.
  gtk_widget_hide(widget);

  // Set the parent.
  static_cast<WidgetGtk*>(GetWidget())->AddChild(widget);
  Layout();

  // TODO: figure out focus.
  // FocusManager::InstallFocusSubclass(
  // hwnd, associated_focus_view()_ ? associated_focus_view() : this);
}

void NativeViewHostGtk::Detach() {
  DCHECK(native_view());

  g_signal_handler_disconnect(G_OBJECT(native_view()), destroy_signal_id_);
  destroy_signal_id_ = 0;

  // TODO: focus.
  // FocusManager::UninstallFocusSubclass(native_view());
  set_native_view(NULL);
  set_installed_clip(false);
}

void NativeViewHostGtk::ViewHierarchyChanged(bool is_add, View* parent,
                                             View* child) {
  if (!native_view())
    return;

  WidgetGtk* parent_widget = static_cast<WidgetGtk*>(GetWidget());
  if (is_add && parent_widget) {
    GtkWidget* widget_parent = gtk_widget_get_parent(native_view());
    GtkWidget* parent_widget_widget = parent_widget->child_widget_parent();
    if (widget_parent != parent_widget_widget) {
      g_object_ref(native_view());
      if (widget_parent)
        gtk_container_remove(GTK_CONTAINER(widget_parent), native_view());
      gtk_container_add(GTK_CONTAINER(parent_widget_widget), native_view());
      g_object_unref(native_view());
    }
    if (IsVisibleInRootView())
      gtk_widget_show(native_view());
    else
      gtk_widget_hide(native_view());
    Layout();
  } else if (!is_add) {
    gtk_widget_hide(native_view());
    if (parent_widget) {
      gtk_container_remove(GTK_CONTAINER(parent_widget->child_widget_parent()),
                           native_view());
    }
  }
}

void NativeViewHostGtk::Focus() {
  NOTIMPLEMENTED();
}

void NativeViewHostGtk::InstallClip(int x, int y, int w, int h) {
  DCHECK(w > 0 && h > 0);

  bool has_window = (GTK_WIDGET_FLAGS(native_view()) & GTK_NO_WINDOW) == 0;
  if (!has_window) {
    // Clip is only supported on GtkWidgets that have windows. If this becomes
    // an issue (as it may be in the options dialog) we'll need to wrap the
    // widget in a GtkFixed with a window. We have to do this as not all widgets
    // support turning on GTK_NO_WINDOW (for example, buttons don't appear to
    // draw anything when they have a window).
    NOTREACHED();
    return;
  }
  DCHECK(has_window);
  // Unset the current region.
  gdk_window_shape_combine_region(native_view()->window, NULL, 0, 0);

  // Set a new region.
  // TODO: using shapes is a bit expensive. Should investigated if there is
  // another more efficient way to accomplish this.
  GdkRectangle clip_rect = { x, y, w, h };
  GdkRegion* clip_region = gdk_region_rectangle(&clip_rect);
  gdk_window_shape_combine_region(native_view()->window, clip_region, x, y);
  gdk_region_destroy(clip_region);
}

void NativeViewHostGtk::UninstallClip() {
  gtk_widget_shape_combine_mask(native_view(), NULL, 0, 0);
}

void NativeViewHostGtk::ShowWidget(int x, int y, int w, int h) {
  WidgetGtk* parent = static_cast<WidgetGtk*>(GetWidget());
  parent->PositionChild(native_view(), x, y, w, h);
  gtk_widget_show(native_view());
}

void NativeViewHostGtk::HideWidget() {
  gtk_widget_hide(native_view());
}

void NativeViewHostGtk::OnDestroy() {
  set_native_view(NULL);
}

// static
void NativeViewHostGtk::CallDestroy(GtkObject* object) {
  View* view = GetViewForNative(GTK_WIDGET(object));
  if (!view)
    return;

  return static_cast<NativeViewHostGtk*>(view)->OnDestroy();
}

}  // namespace views
