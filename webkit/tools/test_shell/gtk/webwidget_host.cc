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
                        WebWidgetHost* host) {
  DLOG(INFO) << "  -- Resize " << config->width << " " << config->height;
  host->Resize(gfx::Size(config->width, config->height));
  return FALSE;
}

gboolean ExposeEvent(GtkWidget* widget, GdkEventExpose* expose,
                     WebWidgetHost* host) {
  host->Paint();
  return FALSE;
}

gboolean DestroyEvent(GtkWidget* widget, GdkEvent* event,
                      WebWidgetHost* host) {
  DLOG(INFO) << "  -- Destroy";
  host->WindowDestroyed();
  return FALSE;
}

gboolean KeyPressReleaseEvent(GtkWidget* widget, GdkEventKey* event,
                              WebWidgetHost* host) {
  DLOG(INFO) << "  -- Key press or release";
  WebKeyboardEvent wke(event);
  host->webwidget()->HandleInputEvent(&wke);
  return FALSE;
}

gboolean FocusIn(GtkWidget* widget, GdkEventFocus* focus,
                 WebWidgetHost* host) {
  host->webwidget()->SetFocus(true);
  return FALSE;
}

gboolean FocusOut(GtkWidget* widget, GdkEventFocus* focus,
                  WebWidgetHost* host) {
  host->webwidget()->SetFocus(false);
  return FALSE;
}

gboolean ButtonPressReleaseEvent(GtkWidget* widget, GdkEventButton* event,
                                 WebWidgetHost* host) {
  DLOG(INFO) << "  -- mouse button press or release";
  WebMouseEvent wme(event);
  host->webwidget()->HandleInputEvent(&wme);
  return FALSE;
}

gboolean MouseMoveEvent(GtkWidget* widget, GdkEventMotion* event,
                        WebWidgetHost* host) {
  WebMouseEvent wme(event);
  host->webwidget()->HandleInputEvent(&wme);
  return FALSE;
}

gboolean MouseScrollEvent(GtkWidget* widget, GdkEventScroll* event,
                          WebWidgetHost* host) {
  WebMouseWheelEvent wmwe(event);
  host->webwidget()->HandleInputEvent(&wmwe);
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
  g_signal_connect(widget, "key-press-event", G_CALLBACK(KeyPressReleaseEvent),
                   host);
  g_signal_connect(widget, "key-release-event",
                   G_CALLBACK(KeyPressReleaseEvent), host);
  g_signal_connect(widget, "focus-in-event", G_CALLBACK(FocusIn), host);
  g_signal_connect(widget, "focus-out-event", G_CALLBACK(FocusOut), host);
  g_signal_connect(widget, "button-press-event",
                   G_CALLBACK(ButtonPressReleaseEvent), host);
  g_signal_connect(widget, "button-release-event",
                   G_CALLBACK(ButtonPressReleaseEvent), host);
  g_signal_connect(widget, "motion-notify-event", G_CALLBACK(MouseMoveEvent),
                   host);
  g_signal_connect(widget, "scroll-event", G_CALLBACK(MouseScrollEvent),
                   host);

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

void WebWidgetHost::DidInvalidateRect(const gfx::Rect& damaged_rect) {
  DLOG_IF(WARNING, painting_) << "unexpected invalidation while painting";

  // If this invalidate overlaps with a pending scroll, then we have to
  // downgrade to invalidating the scroll rect.
  if (damaged_rect.Intersects(scroll_rect_)) {
    paint_rect_ = paint_rect_.Union(scroll_rect_);
    ResetScrollRect();
  }
  paint_rect_ = paint_rect_.Union(damaged_rect);

  gtk_widget_queue_draw_area(GTK_WIDGET(view_), damaged_rect.x(),
      damaged_rect.y(), damaged_rect.width(), damaged_rect.height());
}

void WebWidgetHost::DidScrollRect(int dx, int dy, const gfx::Rect& clip_rect) {
  DCHECK(dx || dy);

  // If we already have a pending scroll operation or if this scroll operation
  // intersects the existing paint region, then just failover to invalidating.
  if (!scroll_rect_.IsEmpty() || paint_rect_.Intersects(clip_rect)) {
    paint_rect_ = paint_rect_.Union(scroll_rect_);
    ResetScrollRect();
    paint_rect_ = paint_rect_.Union(clip_rect);
  }

  // We will perform scrolling lazily, when requested to actually paint.
  scroll_rect_ = clip_rect;
  scroll_dx_ = dx;
  scroll_dy_ = dy;

  gtk_widget_queue_draw_area(GTK_WIDGET(view_), clip_rect.x(), clip_rect.y(),
                             clip_rect.width(), clip_rect.height());
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

  // Allocate a canvas if necessary
  if (!canvas_.get()) {
    ResetScrollRect();
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

  // TODO(agl): Optimized scrolling code would go here.
  ResetScrollRect();

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
  gdk_draw_pixbuf(view_->window, NULL, bitdev->pixbuf(),
                  0, 0, 0, 0, width, height, GDK_RGB_DITHER_NONE, 0, 0);
}

void WebWidgetHost::ResetScrollRect() {
  scroll_rect_ = gfx::Rect();
  scroll_dx_ = 0;
  scroll_dy_ = 0;
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
