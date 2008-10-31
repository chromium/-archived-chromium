// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/tools/test_shell/webwidget_host.h"

#include <gtk/gtk.h>

#include "base/logging.h"
#include "base/gfx/platform_canvas_linux.h"
#include "base/gfx/platform_device_linux.h"
#include "base/gfx/bitmap_platform_device_linux.h"
#include "webkit/glue/webinputevent.h"
#include "webkit/glue/webwidget.h"

namespace {

// -----------------------------------------------------------------------------
// Callback functions to proxy to host...

gboolean ConfigureEvent(GtkWidget* widget, GdkEventConfigure* config,
                        gpointer userdata) {
  WebWidgetHost* host = (WebWidgetHost* ) userdata;
  DLOG(INFO) << "  -- Resize " << config->width << " " << config->height;
  host->Resize(gfx::Size(config->width, config->height));
  return FALSE;
}

gboolean ExposeEvent(GtkWidget* widget, GdkEventExpose* expose,
                     gpointer userdata) {
  WebWidgetHost* host = (WebWidgetHost* ) userdata;
  DLOG(INFO) << "  -- Expose";
  host->Paint();
  return FALSE;
}

gboolean DestroyEvent(GtkWidget* widget, GdkEvent* event, gpointer userdata) {
  WebWidgetHost* host = (WebWidgetHost* ) userdata;
  DLOG(INFO) << "  -- Destroy";
  host->WindowDestroyed();
  return FALSE;
}

gboolean KeyPressEvent(GtkWidget* widget, GdkEventKey* event,
                       gpointer userdata) {
  WebWidgetHost* host = (WebWidgetHost* ) userdata;
  DLOG(INFO) << "  -- Key press";
  WebKeyboardEvent wke(event);
  host->webwidget()->HandleInputEvent(&wke);
  return FALSE;
}

gboolean FocusIn(GtkWidget* widget, GdkEventFocus* focus, gpointer userdata) {
  WebWidgetHost* host = (WebWidgetHost* ) userdata;
  host->webwidget()->SetFocus(true);
  return FALSE;
}

gboolean FocusOut(GtkWidget* widget, GdkEventFocus* focus, gpointer userdata) {
  WebWidgetHost* host = (WebWidgetHost* ) userdata;
  host->webwidget()->SetFocus(false);
  return FALSE;
}

}  // anonymous namespace

// -----------------------------------------------------------------------------

gfx::WindowHandle WebWidgetHost::CreateWindow(gfx::WindowHandle box,
                                              void* host) {
  GtkWidget* widget = gtk_drawing_area_new();
  gtk_box_pack_start(GTK_BOX(box), widget, TRUE, TRUE, 0);

  gtk_widget_add_events(widget, GDK_EXPOSURE_MASK |
                                GDK_POINTER_MOTION_MASK |
                                GDK_BUTTON_PRESS_MASK |
                                GDK_BUTTON_RELEASE_MASK |
                                GDK_KEY_PRESS_MASK |
                                GDK_KEY_RELEASE_MASK);
  // TODO(agl): set GTK_CAN_FOCUS flag
  g_signal_connect(widget, "configure-event", G_CALLBACK(ConfigureEvent), host);
  g_signal_connect(widget, "expose-event", G_CALLBACK(ExposeEvent), host);
  g_signal_connect(widget, "destroy-event", G_CALLBACK(DestroyEvent), host);
  g_signal_connect(widget, "key-press-event", G_CALLBACK(KeyPressEvent), host);
  g_signal_connect(widget, "focus-in-event", G_CALLBACK(FocusIn), host);
  g_signal_connect(widget, "focus-out-event", G_CALLBACK(FocusOut), host);

  return widget;
}

WebWidgetHost* WebWidgetHost::Create(gfx::WindowHandle box,
                                     WebWidgetDelegate* delegate) {
  LOG(INFO) << "In WebWidgetHost::Create";

  WebWidgetHost* host = new WebWidgetHost();
  host->view_ = CreateWindow(box, host);
  host->webwidget_ = WebWidget::Create(delegate);

  return host;
}

void WebWidgetHost::DidInvalidateRect(const gfx::Rect& rect) {
  LOG(INFO) << "  -- Invalidate " << rect.x() << " "
            << rect.y() << " "
            << rect.width() << " "
            << rect.height() << " ";

  gtk_widget_queue_draw_area(GTK_WIDGET(view_), rect.x(), rect.y(), rect.width(),
                             rect.height());
}

void WebWidgetHost::DidScrollRect(int dx, int dy, const gfx::Rect& clip_rect) {
  NOTIMPLEMENTED();
}

WebWidgetHost* FromWindow(gfx::WindowHandle view) {
  const gpointer p = g_object_get_data(G_OBJECT(view), "webwidgethost");
  return (WebWidgetHost* ) p;
}

WebWidgetHost::WebWidgetHost()
    : view_(NULL),
      webwidget_(NULL),
      scroll_dx_(0),
      scroll_dy_(0),
      track_mouse_leave_(false) {
  set_painting(false);
}

WebWidgetHost::~WebWidgetHost() {
  webwidget_->Close();
  webwidget_->Release();
}

void WebWidgetHost::Resize(const gfx::Size &newsize) {
  // The pixel buffer backing us is now the wrong size
  canvas_.reset();

  gtk_widget_set_size_request(GTK_WIDGET(view_), newsize.width(), newsize.height());
  webwidget_->Resize(gfx::Size(newsize.width(), newsize.height()));
}

void WebWidgetHost::Paint() {
  gint width, height;
  gtk_widget_get_size_request(GTK_WIDGET(view_), &width, &height);

  gfx::Rect client_rect(width, height);

  if (!canvas_.get()) {
    paint_rect_ = client_rect;
    canvas_.reset(new gfx::PlatformCanvas(width, height, true));
    if (!canvas_.get()) {
      // memory allocation failed, we can't paint.
      LOG(ERROR) << "Failed to allocate memory for " << width << "x" << height;
      return;
    }
  }

  // This may result in more invalidation
  webwidget_->Layout();

  // TODO(agl): scrolling code

  // Paint the canvas if necessary.  Allow painting to generate extra rects the
  // first time we call it.  This is necessary because some WebCore rendering
  // objects update their layout only when painted.
  
  for (int i = 0; i < 2; ++i) {
    paint_rect_ = client_rect.Intersect(paint_rect_);
    if (!paint_rect_.IsEmpty()) {
      gfx::Rect rect(paint_rect_);
      paint_rect_ = gfx::Rect();

      DLOG_IF(WARNING, i == 1) << "painting caused additional invalidations";
      PaintRect(rect);
    }
  }
  DCHECK(paint_rect_.IsEmpty());

  // BitBlit to the X server
  gfx::PlatformDeviceLinux &platdev = canvas_->getTopPlatformDevice();
  gfx::BitmapPlatformDeviceLinux* const bitdev =
    static_cast<gfx::BitmapPlatformDeviceLinux* >(&platdev);
  LOG(INFO) << "Using pixel data at " << (void *) gdk_pixbuf_get_pixels(bitdev->pixbuf());
  gdk_draw_pixbuf(view_->window, NULL, bitdev->pixbuf(),
                  0, 0, 0, 0, width, height, GDK_RGB_DITHER_NONE, 0, 0);
}

void WebWidgetHost::PaintRect(const gfx::Rect& rect) {
  set_painting(true);
  webwidget_->Paint(canvas_.get(), rect);
  set_painting(false);
}

// -----------------------------------------------------------------------------
// This is called when the GTK window is destroyed. In the Windows code this
// deletes this object. Since it's only test_shell it probably doesn't matter
// that much.
// -----------------------------------------------------------------------------
void WebWidgetHost::WindowDestroyed() {
  delete this;
}
