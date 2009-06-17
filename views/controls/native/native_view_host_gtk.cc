// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/controls/native/native_view_host_gtk.h"

#include <gtk/gtk.h>

#include "base/logging.h"
#include "views/controls/native/native_view_host.h"
#include "views/widget/widget_gtk.h"

namespace views {

////////////////////////////////////////////////////////////////////////////////
// NativeViewHostGtk, public:

NativeViewHostGtk::NativeViewHostGtk(NativeViewHost* host)
    : host_(host),
      installed_clip_(false),
      destroy_signal_id_(0) {
}

NativeViewHostGtk::~NativeViewHostGtk() {
}

////////////////////////////////////////////////////////////////////////////////
// NativeViewHostGtk, NativeViewHostWrapper implementation:

void NativeViewHostGtk::NativeViewAttached() {
  DCHECK(host_->native_view());

  if (gtk_widget_get_parent(host_->native_view()))
    GetHostWidget()->ReparentChild(host_->native_view());
  else
    GetHostWidget()->AddChild(host_->native_view());

  if (!destroy_signal_id_) {
    destroy_signal_id_ = g_signal_connect(G_OBJECT(host_->native_view()),
                                          "destroy", G_CALLBACK(CallDestroy),
                                          this);
  }

  // Always layout though.
  host_->Layout();

  // TODO(port): figure out focus.
}

void NativeViewHostGtk::NativeViewDetaching() {
  DCHECK(host_->native_view());

  g_signal_handler_disconnect(G_OBJECT(host_->native_view()),
                              destroy_signal_id_);
  destroy_signal_id_ = 0;

  // TODO(port): focus.
  // FocusManager::UninstallFocusSubclass(native_view());
  installed_clip_ = false;
}

void NativeViewHostGtk::AddedToWidget() {
  if (!host_->native_view())
    return;
  WidgetGtk* parent_widget = GetHostWidget();
  GtkWidget* widget_parent = gtk_widget_get_parent(host_->native_view());
  GtkWidget* parent_widget_widget = parent_widget->window_contents();
  if (widget_parent != parent_widget_widget) {
    g_object_ref(host_->native_view());
    if (widget_parent)
      gtk_container_remove(GTK_CONTAINER(widget_parent), host_->native_view());
    gtk_container_add(GTK_CONTAINER(parent_widget_widget),
                      host_->native_view());
    g_object_unref(host_->native_view());
  }
  if (host_->IsVisibleInRootView())
    gtk_widget_show(host_->native_view());
  else
    gtk_widget_hide(host_->native_view());
  host_->Layout();
}

void NativeViewHostGtk::RemovedFromWidget() {
  if (!host_->native_view())
    return;

  WidgetGtk* parent_widget = GetHostWidget();
  gtk_widget_hide(host_->native_view());
  if (parent_widget) {
    gtk_container_remove(GTK_CONTAINER(parent_widget->window_contents()),
                         host_->native_view());
  }
}

void NativeViewHostGtk::InstallClip(int x, int y, int w, int h) {
  DCHECK(w > 0 && h > 0);

  bool has_window =
      (GTK_WIDGET_FLAGS(host_->native_view()) & GTK_NO_WINDOW) == 0;
  if (!has_window) {
    // Clip is only supported on GtkWidgets that have windows. If this becomes
    // an issue (as it may be in the options dialog) we'll need to wrap the
    // widget in a GtkFixed with a window. We have to do this as not all widgets
    // support turning on GTK_NO_WINDOW (for example, buttons don't appear to
    // draw anything when they have a window).
    // NOTREACHED();
    return;
  }
  DCHECK(has_window);
  // Unset the current region.
  gdk_window_shape_combine_region(host_->native_view()->window, NULL, 0, 0);

  // Set a new region.
  // TODO: using shapes is a bit expensive. Should investigated if there is
  // another more efficient way to accomplish this.
  GdkRectangle clip_rect = { x, y, w, h };
  GdkRegion* clip_region = gdk_region_rectangle(&clip_rect);
  gdk_window_shape_combine_region(host_->native_view()->window, clip_region, x,
                                  y);
  gdk_region_destroy(clip_region);
  installed_clip_ = true;
}

bool NativeViewHostGtk::HasInstalledClip() {
  return installed_clip_;
}

void NativeViewHostGtk::UninstallClip() {
  gtk_widget_shape_combine_mask(host_->native_view(), NULL, 0, 0);
  installed_clip_ = false;
}

void NativeViewHostGtk::ShowWidget(int x, int y, int w, int h) {
  GetHostWidget()->PositionChild(host_->native_view(), x, y, w, h);
  gtk_widget_show(host_->native_view());
}

void NativeViewHostGtk::HideWidget() {
  gtk_widget_hide(host_->native_view());
}

void NativeViewHostGtk::SetFocus() {
  NOTIMPLEMENTED();
}

////////////////////////////////////////////////////////////////////////////////
// NativeViewHostGtk, private:

WidgetGtk* NativeViewHostGtk::GetHostWidget() const {
  return static_cast<WidgetGtk*>(host_->GetWidget());
}

// static
void NativeViewHostGtk::CallDestroy(GtkObject* object,
                                    NativeViewHostGtk* host) {
  return host->host_->NativeViewDestroyed();
}

////////////////////////////////////////////////////////////////////////////////
// NativeViewHostWrapper, public:

// static
NativeViewHostWrapper* NativeViewHostWrapper::CreateWrapper(
    NativeViewHost* host) {
  return new NativeViewHostGtk(host);
}

}  // namespace views
