// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/tools/test_shell/webwidget_host.h"

#include "base/logging.h"
#include "base/gfx/platform_canvas_linux.h"
#include "base/gfx/platform_device_linux.h"
#include "base/gfx/bitmap_platform_device_linux.h"
#include "webkit/glue/webinputevent.h"
#include "webkit/glue/webwidget.h"

#include <gtk/gtk.h>

namespace {

// -----------------------------------------------------------------------------
// Callback functions to proxy to host...

static gboolean configureEvent(GtkWidget* widget, GdkEventConfigure* config,
                               gpointer userdata) {
  WebWidgetHost* host = (WebWidgetHost* ) userdata;
  DLOG(INFO) << "  -- Resize " << config->width << " " << config->height;
  host->Resize(gfx::Size(config->width, config->height));
  return FALSE;
}

static gboolean exposeEvent(GtkWidget* widget, GdkEventExpose* expose,
                            gpointer userdata) {
  WebWidgetHost* host = (WebWidgetHost* ) userdata;
  DLOG(INFO) << "  -- Expose";
  host->Paint();
  return FALSE;
}

static gboolean destroyEvent(GtkWidget* widget, GdkEvent* event, gpointer userdata) {
  WebWidgetHost* host = (WebWidgetHost* ) userdata;
  DLOG(INFO) << "  -- Destroy";
  host->WindowDestroyed();
  return FALSE;
}

static gboolean keyPressEvent(GtkWidget* widget, GdkEventKey* event,
                              gpointer userdata) {
  WebWidgetHost* host = (WebWidgetHost* ) userdata;
  DLOG(INFO) << "  -- Key press";
  WebKeyboardEvent wke(event);
  host->webwidget()->HandleInputEvent(&wke);
  return FALSE;
}

static gboolean focusIn(GtkWidget* widget, GdkEventFocus* focus, gpointer userdata) {
  WebWidgetHost* host = (WebWidgetHost* ) userdata;
  host->webwidget()->SetFocus(true);
  return FALSE;
}

static gboolean focusOut(GtkWidget* widget, GdkEventFocus* focus, gpointer userdata) {
  WebWidgetHost* host = (WebWidgetHost* ) userdata;
  host->webwidget()->SetFocus(false);
  return FALSE;
}

}  // anonymous namespace

// -----------------------------------------------------------------------------

WebWidgetHost* WebWidgetHost::Create(gfx::WindowHandle box,
                                     WebWidgetDelegate* delegate) {
  WebWidgetHost* host = new WebWidgetHost();
  host->view_ = gtk_drawing_area_new();
  gtk_widget_add_events(host->view_, GDK_EXPOSURE_MASK |
                                     GDK_POINTER_MOTION_MASK |
                                     GDK_BUTTON_PRESS_MASK |
                                     GDK_BUTTON_RELEASE_MASK |
                                     GDK_KEY_PRESS_MASK |
                                     GDK_KEY_RELEASE_MASK);
  // TODO(agl): set GTK_CAN_FOCUS flag
  host->webwidget_ = WebWidget::Create(delegate);
  g_object_set_data(G_OBJECT(host->view_), "webwidgethost", host);

  g_signal_connect(host->view_, "configure-event", G_CALLBACK(configureEvent), host);
  g_signal_connect(host->view_, "expose-event", G_CALLBACK(exposeEvent), host);
  g_signal_connect(host->view_, "destroy-event", G_CALLBACK(destroyEvent), host);
  g_signal_connect(host->view_, "key-press-event", G_CALLBACK(keyPressEvent), host);
  g_signal_connect(host->view_, "focus-in-event", G_CALLBACK(focusIn), host);
  g_signal_connect(host->view_, "focus-out-event", G_CALLBACK(focusOut), host);

  gtk_box_pack_start(GTK_BOX(box), host->view_, TRUE, TRUE, 0);

  return host;
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
  gdk_pixbuf_save(bitdev->pixbuf(), "output.png", "png", NULL, NULL);
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
