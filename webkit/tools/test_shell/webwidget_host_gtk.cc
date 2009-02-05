// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/tools/test_shell/webwidget_host.h"

#include <cairo/cairo.h>
#include <gtk/gtk.h>

#include "base/logging.h"
#include "skia/ext/bitmap_platform_device_linux.h"
#include "skia/ext/platform_canvas_linux.h"
#include "skia/ext/platform_device_linux.h"
#include "webkit/glue/webinputevent.h"
#include "webkit/glue/webwidget.h"

namespace {

// The web contents are completely drawn and handled by WebKit, except that
// windowed plugins are GtkSockets on top of it.  GtkSockets are unhappy
// if they cannot be put within a parent container, so what we want is basically
// a GtkDrawingArea that is also a GtkContainer.  The existing Gtk classes for
// this sort of thing (reasonably) assume that the positioning of the children
// will be handled by the parent, which isn't the case for us where WebKit
// drives everything.  So we create a custom widget that is just a GtkContainer
// that contains a native window.

// Sends a "configure" event to the widget, allowing it to repaint.
// Used internally by the WebWidgetHostGtk implementation.
void WebWidgetHostGtkSendConfigure(GtkWidget* widget) {
  GdkEvent* event = gdk_event_new(GDK_CONFIGURE);
  // This ref matches GtkDrawingArea code.  It must be balanced by the
  // event-handling code.
  g_object_ref(widget->window);
  event->configure.window = widget->window;
  event->configure.send_event = TRUE;
  event->configure.x = widget->allocation.x;
  event->configure.y = widget->allocation.y;
  event->configure.width = widget->allocation.width;
  event->configure.height = widget->allocation.height;
  gtk_widget_event(widget, event);
  gdk_event_free(event);
}

// Implementation of the "realize" event for the custom WebWidgetHostGtk
// widget.  Creates an X window.  (Nearly identical to GtkDrawingArea code.)
void WebWidgetHostGtkRealize(GtkWidget* widget) {
  GTK_WIDGET_SET_FLAGS(widget, GTK_REALIZED);

  GdkWindowAttr attributes;
  attributes.window_type = GDK_WINDOW_CHILD;
  attributes.x = widget->allocation.x;
  attributes.y = widget->allocation.y;
  attributes.width = widget->allocation.width;
  attributes.height = widget->allocation.height;
  attributes.wclass = GDK_INPUT_OUTPUT;
  attributes.visual = gtk_widget_get_visual(widget);
  attributes.colormap = gtk_widget_get_colormap(widget);
  attributes.event_mask = gtk_widget_get_events(widget) | GDK_EXPOSURE_MASK;
  int attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL | GDK_WA_COLORMAP;

  widget->window = gdk_window_new(gtk_widget_get_parent_window(widget),
                                  &attributes, attributes_mask);
  gdk_window_set_user_data(widget->window, widget);

  WebWidgetHostGtkSendConfigure(widget);
}

// Implementation of the "size-allocate" event for the custom WebWidgetHostGtk
// widget.  Resizes the window.  (Nearly identical to GtkDrawingArea code.)
void WebWidgetHostGtkSizeAllocate(GtkWidget* widget,
                                  GtkAllocation* allocation) {
  widget->allocation = *allocation;
  if (!GTK_WIDGET_REALIZED(widget)) {
    // We don't have a window yet, so nothing to do.
    return;
  }

  gdk_window_move_resize(widget->window,
                         allocation->x, allocation->y,
                         allocation->width, allocation->height);

  WebWidgetHostGtkSendConfigure(widget);
}

// Implementation of "remove" for our GtkContainer subclass.
// This called when plugins shut down.  We can just ignore it.
void WebWidgetHostGtkRemove(GtkContainer* container, GtkWidget* widget) {
  // Do nothing.
}

// Implementation of the class init function for WebWidgetHostGtk.
void WebWidgetHostGtkClassInit(GtkContainerClass* container_class) {
  GtkWidgetClass* widget_class =
      reinterpret_cast<GtkWidgetClass*>(container_class);
  widget_class->realize = WebWidgetHostGtkRealize;
  widget_class->size_allocate = WebWidgetHostGtkSizeAllocate;

  container_class->remove = WebWidgetHostGtkRemove;
}

// Constructs the GType for the custom Gtk widget.
// (Gtk boilerplate, basically.)
GType WebWidgetHostGtkGetType() {
  static GType type = 0;
  if (!type) {
    static const GTypeInfo info = {
      sizeof(GtkContainerClass),
      NULL, NULL,
      (GClassInitFunc)WebWidgetHostGtkClassInit,
      NULL, NULL,
      sizeof(GtkContainer),  // We are identical in memory to a GtkContainer.
      0, NULL,
    };
    type = g_type_register_static(GTK_TYPE_CONTAINER, "WebWidgetHostGtk",
                                  &info, (GTypeFlags)0);
  }
  return type;
}

// Create a new WebWidgetHostGtk.
GtkWidget* WebWidgetHostGtkNew() {
  return GTK_WIDGET(g_object_new(WebWidgetHostGtkGetType(), NULL));
}

// In response to an invalidation, we call into WebKit to do layout. On
// Windows, WM_PAINT is a virtual message so any extra invalidates that come up
// while it's doing layout are implicitly swallowed as soon as we actually do
// drawing via BeginPaint.
//
// Though GTK does know how to collapse multiple paint requests, it won't erase
// paint requests from the future when we start drawing.  To avoid an infinite
// cycle of repaints, we track whether we're currently handling a redraw, and
// during that if we get told by WebKit that a region has become invalid, we
// still add that region to the local dirty rect but *don't* enqueue yet
// another "do a paint" message.
bool handling_expose = false;

// -----------------------------------------------------------------------------
// Callback functions to proxy to host...

gboolean ConfigureEvent(GtkWidget* widget, GdkEventConfigure* config,
                        WebWidgetHost* host) {
  host->Resize(gfx::Size(config->width, config->height));
  return FALSE;
}

gboolean ExposeEvent(GtkWidget* widget, GdkEventExpose* expose,
                     WebWidgetHost* host) {
  // See comments above about what handling_expose is for.
  handling_expose = true;
  gfx::Rect rect(expose->area);
  host->UpdatePaintRect(rect);
  host->Paint();
  handling_expose = false;
  return FALSE;
}

gboolean WindowDestroyed(GtkWidget* widget, WebWidgetHost* host) {
  host->WindowDestroyed();
  return FALSE;
}

gboolean KeyPressReleaseEvent(GtkWidget* widget, GdkEventKey* event,
                              WebWidgetHost* host) {
  WebKeyboardEvent wke(event);
  host->webwidget()->HandleInputEvent(&wke);

  // The WebKeyboardEvent model, when holding down a key, is:
  //   KEY_DOWN, CHAR, (repeated CHAR as key repeats,) KEY_UP
  // The GDK model for the same sequence is just:
  //   KEY_PRESS, (repeated KEY_PRESS as key repeats,) KEY_RELEASE
  // So we must simulate a CHAR event for every key press.
  if (event->type == GDK_KEY_PRESS) {
    wke.type = WebKeyboardEvent::CHAR;
    host->webwidget()->HandleInputEvent(&wke);
  }

  return FALSE;
}

// This signal is called when arrow keys or tab is pressed.  If we return true,
// we prevent focus from being moved to another widget.  If we want to allow
// focus to be moved outside of web contents, we need to implement
// WebViewDelegate::TakeFocus in the test webview delegate.
gboolean FocusMove(GtkWidget* widget, GdkEventFocus* focus,
                   WebWidgetHost* host) {
  return TRUE;
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

GtkWidget* WebWidgetHost::CreateWindow(GtkWidget* parent_view,
                                       void* host) {
  GtkWidget* widget = WebWidgetHostGtkNew();

  gtk_box_pack_start(GTK_BOX(parent_view), widget, TRUE, TRUE, 0);

  // TODO(evanm): it might be cleaner to just have the custom widget set
  // all of this up rather than connecting signals.
  gtk_widget_add_events(widget, GDK_EXPOSURE_MASK |
                                GDK_POINTER_MOTION_MASK |
                                GDK_BUTTON_PRESS_MASK |
                                GDK_BUTTON_RELEASE_MASK |
                                GDK_KEY_PRESS_MASK |
                                GDK_KEY_RELEASE_MASK);
  GTK_WIDGET_SET_FLAGS(widget, GTK_CAN_FOCUS);
  g_signal_connect(widget, "configure-event", G_CALLBACK(ConfigureEvent), host);
  g_signal_connect(widget, "expose-event", G_CALLBACK(ExposeEvent), host);
  g_signal_connect(widget, "destroy", G_CALLBACK(::WindowDestroyed), host);
  g_signal_connect(widget, "key-press-event", G_CALLBACK(KeyPressReleaseEvent),
                   host);
  g_signal_connect(widget, "key-release-event",
                   G_CALLBACK(KeyPressReleaseEvent), host);
  g_signal_connect(widget, "focus", G_CALLBACK(FocusMove), host);
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

WebWidgetHost* WebWidgetHost::Create(GtkWidget* parent_view,
                                     WebWidgetDelegate* delegate) {
  WebWidgetHost* host = new WebWidgetHost();
  host->view_ = CreateWindow(parent_view, host);
  host->webwidget_ = WebWidget::Create(delegate);
  // We manage our own double buffering because we need to be able to update
  // the expose area in an ExposeEvent within the lifetime of the event handler.
  gtk_widget_set_double_buffered(GTK_WIDGET(host->view_), false);

  return host;
}

void WebWidgetHost::UpdatePaintRect(const gfx::Rect& rect) {
  paint_rect_ = paint_rect_.Union(rect);
}

void WebWidgetHost::DidInvalidateRect(const gfx::Rect& damaged_rect) {
  DLOG_IF(WARNING, painting_) << "unexpected invalidation while painting";

  UpdatePaintRect(damaged_rect);

  if (!handling_expose) {
    gtk_widget_queue_draw_area(GTK_WIDGET(view_), damaged_rect.x(),
        damaged_rect.y(), damaged_rect.width(), damaged_rect.height());
  }
}

void WebWidgetHost::DidScrollRect(int dx, int dy, const gfx::Rect& clip_rect) {
  // This is used for optimizing painting when the renderer is scrolled. We're
  // currently not doing any optimizations so just invalidate the region.
  DidInvalidateRect(clip_rect);
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

  webwidget_->Resize(gfx::Size(newsize.width(), newsize.height()));
}

void WebWidgetHost::Paint() {
  int width = view_->allocation.width;
  int height = view_->allocation.height;
  gfx::Rect client_rect(width, height);

  // Allocate a canvas if necessary
  if (!canvas_.get()) {
    ResetScrollRect();
    paint_rect_ = client_rect;
    canvas_.reset(new skia::PlatformCanvas(width, height, true));
    if (!canvas_.get()) {
      // memory allocation failed, we can't paint.
      LOG(ERROR) << "Failed to allocate memory for " << width << "x" << height;
      return;
    }
  }

  // This may result in more invalidation
  webwidget_->Layout();

  // Paint the canvas if necessary.  Allow painting to generate extra rects the
  // first time we call it.  This is necessary because some WebCore rendering
  // objects update their layout only when painted.
  // Store the total area painted in total_paint. Then tell the gdk window
  // to update that area after we're done painting it.
  gfx::Rect total_paint;
  for (int i = 0; i < 2; ++i) {
    paint_rect_ = client_rect.Intersect(paint_rect_);
    if (!paint_rect_.IsEmpty()) {
      gfx::Rect rect(paint_rect_);
      paint_rect_ = gfx::Rect();

      DLOG_IF(WARNING, i == 1) << "painting caused additional invalidations";
      PaintRect(rect);
      total_paint = total_paint.Union(rect);
    }
  }
  DCHECK(paint_rect_.IsEmpty());

  // Invalidate the paint region on the widget's underlying gdk window. Note
  // that gdk_window_invalidate_* will generate extra expose events, which
  // we wish to avoid. So instead we use calls to begin_paint/end_paint.
  GdkRectangle grect = {
      total_paint.x(),
      total_paint.y(),
      total_paint.width(),
      total_paint.height(),
  };
  GdkWindow* window = view_->window;
  gdk_window_begin_paint_rect(window, &grect);

  // BitBlit to the gdk window.
  skia::PlatformDeviceLinux &platdev = canvas_->getTopPlatformDevice();
  skia::BitmapPlatformDeviceLinux* const bitdev =
      static_cast<skia::BitmapPlatformDeviceLinux* >(&platdev);
  cairo_t* cairo_drawable = gdk_cairo_create(window);
  cairo_set_source_surface(cairo_drawable, bitdev->surface(), 0, 0);
  cairo_paint(cairo_drawable);
  cairo_destroy(cairo_drawable);

  gdk_window_end_paint(window);
}

void WebWidgetHost::ResetScrollRect() {
  // This method is only needed for optimized scroll painting, which we don't
  // care about in the test shell, yet.
}

void WebWidgetHost::PaintRect(const gfx::Rect& rect) {
  set_painting(true);
  webwidget_->Paint(canvas_.get(), rect);
  set_painting(false);
}

void WebWidgetHost::WindowDestroyed() {
  delete this;
}
