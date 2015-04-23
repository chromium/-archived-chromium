// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/gtk/focus_store_gtk.h"

#include <gtk/gtk.h>

#include "base/logging.h"
#include "chrome/common/platform_util.h"

FocusStoreGtk::FocusStoreGtk() : widget_(NULL) {
}

FocusStoreGtk::~FocusStoreGtk() {
  DisconnectDestroyHandler();
}

void FocusStoreGtk::Store(GtkWidget* widget) {
  DisconnectDestroyHandler();
  if (!widget) {
    widget_ = NULL;
    return;
  }

  GtkWindow* window = platform_util::GetTopLevel(widget);
  if (!window) {
    widget_ = NULL;
    return;
  }

  widget_ = window->focus_widget;
  if (widget_) {
    // When invoked, |gtk_widget_destroyed| will set |widget_| to NULL.
    destroy_handler_id_ = g_signal_connect(widget_, "destroy",
                                           G_CALLBACK(gtk_widget_destroyed),
                                           &widget_);
  }
}

void FocusStoreGtk::DisconnectDestroyHandler() {
  if (widget_)
    g_signal_handler_disconnect(widget_, destroy_handler_id_);
}
