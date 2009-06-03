// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/gtk/toolbar_star_toggle_gtk.h"

#include "app/resource_bundle.h"
#include "base/gfx/rect.h"
#include "chrome/browser/gtk/bookmark_bubble_gtk.h"
#include "chrome/browser/gtk/browser_toolbar_gtk.h"
#include "grit/theme_resources.h"

ToolbarStarToggleGtk::ToolbarStarToggleGtk(BrowserToolbarGtk* host)
    : host_(host),
      widget_(gtk_button_new()),
      is_starred_(false),
      unstarred_(IDR_STAR, IDR_STAR_P, IDR_STAR_H, IDR_STAR_D),
      starred_(IDR_STARRED, IDR_STARRED_P, IDR_STARRED_H, 0) {
  gtk_widget_set_size_request(widget_.get(),
                             gdk_pixbuf_get_width(unstarred_.pixbufs(0)),
                             gdk_pixbuf_get_height(unstarred_.pixbufs(0)));

  gtk_widget_set_app_paintable(widget_.get(), TRUE);
  // We effectively double-buffer by virtue of having only one image...
  gtk_widget_set_double_buffered(widget_.get(), FALSE);
  g_signal_connect(G_OBJECT(widget_.get()), "expose-event",
                   G_CALLBACK(OnExpose), this);
  GTK_WIDGET_UNSET_FLAGS(widget_.get(), GTK_CAN_FOCUS);
}

ToolbarStarToggleGtk::~ToolbarStarToggleGtk() {
  widget_.Destroy();
}

void ToolbarStarToggleGtk::ShowStarBubble(const GURL& url,
                                          bool newly_bookmarked) {
  GtkWidget* widget = widget_.get();
  gint x, y;
  gdk_window_get_origin(widget->window, &x, &y);
  x += widget->allocation.x;
  y += widget->allocation.y;
  gint width = widget->allocation.width;
  gint height = widget->allocation.height;

  BookmarkBubbleGtk::Show(GTK_WINDOW(gtk_widget_get_toplevel(widget)),
                          gfx::Rect(x, y, width, height),
                          host_->profile(),
                          url,
                          newly_bookmarked);
}

void ToolbarStarToggleGtk::SetStarred(bool starred) {
  is_starred_ = starred;
  gtk_widget_queue_draw(widget_.get());
}

// static
gboolean ToolbarStarToggleGtk::OnExpose(GtkWidget* widget, GdkEventExpose* e,
                                        ToolbarStarToggleGtk* button) {
  if (button->is_starred_) {
    return button->starred_.OnExpose(widget, e);
  } else {
    return button->unstarred_.OnExpose(widget, e);
  }
}
