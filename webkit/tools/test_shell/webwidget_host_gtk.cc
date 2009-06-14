// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/tools/test_shell/webwidget_host.h"

#include <cairo/cairo.h>
#include <gtk/gtk.h>

#include "base/basictypes.h"
#include "base/logging.h"
#include "skia/ext/bitmap_platform_device.h"
#include "skia/ext/platform_canvas.h"
#include "skia/ext/platform_device.h"
#include "webkit/api/public/gtk/WebInputEventFactory.h"
#include "webkit/api/public/x11/WebScreenInfoFactory.h"
#include "webkit/api/public/WebInputEvent.h"
#include "webkit/api/public/WebScreenInfo.h"
#include "webkit/api/public/WebSize.h"
#include "webkit/glue/webwidget.h"
#include "webkit/tools/test_shell/test_shell.h"
#include "webkit/tools/test_shell/test_shell_x11.h"

using WebKit::WebInputEventFactory;
using WebKit::WebKeyboardEvent;
using WebKit::WebMouseEvent;
using WebKit::WebMouseWheelEvent;
using WebKit::WebScreenInfo;
using WebKit::WebScreenInfoFactory;
using WebKit::WebSize;

namespace {

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
bool g_handling_expose = false;

// -----------------------------------------------------------------------------
// Callback functions to proxy to host...

// The web contents are completely drawn and handled by WebKit, except that
// windowed plugins are GtkSockets on top of it.  We need to place the
// GtkSockets inside a GtkContainer.  We use a GtkFixed container, and the
// GtkSocket objects override a little bit to manage their size (see the code
// in webplugin_delegate_impl_gtk.cc).  We listen on a the events we're
// interested in and forward them on to the WebWidgetHost.  This class is a
// collection of static methods, implementing the widget related code.
class WebWidgetHostGtkWidget {
 public:
  // This will create a new widget used for hosting the web contents.  We use
  // our GtkDrawingAreaContainer here, for the reasons mentioned above.
  static GtkWidget* CreateNewWidget(GtkWidget* parent_view,
                                    WebWidgetHost* host) {
    GtkWidget* widget = gtk_fixed_new();
    gtk_fixed_set_has_window(GTK_FIXED(widget), true);

    gtk_box_pack_start(GTK_BOX(parent_view), widget, TRUE, TRUE, 0);

    gtk_widget_add_events(widget, GDK_EXPOSURE_MASK |
                                  GDK_POINTER_MOTION_MASK |
                                  GDK_BUTTON_PRESS_MASK |
                                  GDK_BUTTON_RELEASE_MASK |
                                  GDK_KEY_PRESS_MASK |
                                  GDK_KEY_RELEASE_MASK);
    GTK_WIDGET_SET_FLAGS(widget, GTK_CAN_FOCUS);
    g_signal_connect(widget, "size-request",
                     G_CALLBACK(&HandleSizeRequest), host);
    g_signal_connect(widget, "size-allocate",
                     G_CALLBACK(&HandleSizeAllocate), host);
    g_signal_connect(widget, "configure-event",
                     G_CALLBACK(&HandleConfigure), host);
    g_signal_connect(widget, "expose-event",
                     G_CALLBACK(&HandleExpose), host);
    g_signal_connect(widget, "destroy",
                     G_CALLBACK(&HandleDestroy), host);
    g_signal_connect(widget, "key-press-event",
                     G_CALLBACK(&HandleKeyPress), host);
    g_signal_connect(widget, "key-release-event",
                     G_CALLBACK(&HandleKeyRelease), host);
    g_signal_connect(widget, "focus",
                     G_CALLBACK(&HandleFocus), host);
    g_signal_connect(widget, "focus-in-event",
                     G_CALLBACK(&HandleFocusIn), host);
    g_signal_connect(widget, "focus-out-event",
                     G_CALLBACK(&HandleFocusOut), host);
    g_signal_connect(widget, "button-press-event",
                     G_CALLBACK(&HandleButtonPress), host);
    g_signal_connect(widget, "button-release-event",
                     G_CALLBACK(&HandleButtonRelease), host);
    g_signal_connect(widget, "motion-notify-event",
                     G_CALLBACK(&HandleMotionNotify), host);
    g_signal_connect(widget, "scroll-event",
                     G_CALLBACK(&HandleScroll), host);

    return widget;
  }

 private:
  // Our size was requested.  We let the GtkFixed do its normal calculation,
  // after which this callback is called.  The GtkFixed will come up with a
  // requisition based on its children, which include plugin windows.  Since
  // we don't want to prevent resizing smaller than a plugin window, we need to
  // control the size ourself.
  static void HandleSizeRequest(GtkWidget* widget,
                                GtkRequisition* req,
                                WebWidgetHost* host) {
    // This is arbitrary, but the WebKit scrollbars try to shrink themselves
    // if the browser window is too small.  Give them some space.
    static const int kMinWidthHeight = 64;

    req->width = kMinWidthHeight;
    req->height = kMinWidthHeight;
  }

  // Our size has changed.
  static void HandleSizeAllocate(GtkWidget* widget,
                                 GtkAllocation* allocation,
                                 WebWidgetHost* host) {
    host->Resize(WebSize(allocation->width, allocation->height));
  }

  // Size, position, or stacking of the GdkWindow changed.
  static gboolean HandleConfigure(GtkWidget* widget,
                                  GdkEventConfigure* config,
                                  WebWidgetHost* host) {
    host->Resize(WebSize(config->width, config->height));
    return FALSE;
  }

  // A portion of the GdkWindow needs to be redraw.
  static gboolean HandleExpose(GtkWidget* widget,
                               GdkEventExpose* expose,
                               WebWidgetHost* host) {
    // See comments above about what g_handling_expose is for.
    g_handling_expose = true;
    gfx::Rect rect(expose->area);
    host->UpdatePaintRect(rect);
    host->Paint();
    g_handling_expose = false;
    return FALSE;
  }

  // The GdkWindow was destroyed.
  static gboolean HandleDestroy(GtkWidget* widget, WebWidgetHost* host) {
    host->WindowDestroyed();
    return FALSE;
  }

  // Keyboard key pressed.
  static gboolean HandleKeyPress(GtkWidget* widget,
                                 GdkEventKey* event,
                                 WebWidgetHost* host) {
    const WebKeyboardEvent& wke = WebInputEventFactory::keyboardEvent(event);
    host->webwidget()->HandleInputEvent(&wke);

    return FALSE;
  }

  // Keyboard key released.
  static gboolean HandleKeyRelease(GtkWidget* widget,
                                   GdkEventKey* event,
                                   WebWidgetHost* host) {
    return HandleKeyPress(widget, event, host);
  }

  // This signal is called when arrow keys or tab is pressed.  If we return
  // true, we prevent focus from being moved to another widget.  If we want to
  // allow focus to be moved outside of web contents, we need to implement
  // WebViewDelegate::TakeFocus in the test webview delegate.
  static gboolean HandleFocus(GtkWidget* widget,
                              GdkEventFocus* focus,
                              WebWidgetHost* host) {
    return TRUE;
  }

  // Keyboard focus entered.
  static gboolean HandleFocusIn(GtkWidget* widget,
                                GdkEventFocus* focus,
                                WebWidgetHost* host) {
    // Ignore focus calls in layout test mode so that tests don't mess with each
    // other's focus when running in parallel.
    if (!TestShell::layout_test_mode())
      host->webwidget()->SetFocus(true);
    return FALSE;
  }

  // Keyboard focus left.
  static gboolean HandleFocusOut(GtkWidget* widget,
                                 GdkEventFocus* focus,
                                 WebWidgetHost* host) {
    // Ignore focus calls in layout test mode so that tests don't mess with each
    // other's focus when running in parallel.
    if (!TestShell::layout_test_mode())
      host->webwidget()->SetFocus(false);
    return FALSE;
  }

  // Mouse button down.
  static gboolean HandleButtonPress(GtkWidget* widget,
                                    GdkEventButton* event,
                                    WebWidgetHost* host) {
    const WebMouseEvent& wme = WebInputEventFactory::mouseEvent(event);
    host->webwidget()->HandleInputEvent(&wme);
    return FALSE;
  }

  // Mouse button up.
  static gboolean HandleButtonRelease(GtkWidget* widget,
                                      GdkEventButton* event,
                                      WebWidgetHost* host) {
    return HandleButtonPress(widget, event, host);
  }

  // Mouse pointer movements.
  static gboolean HandleMotionNotify(GtkWidget* widget,
                                     GdkEventMotion* event,
                                     WebWidgetHost* host) {
    const WebMouseEvent& wme = WebInputEventFactory::mouseEvent(event);
    host->webwidget()->HandleInputEvent(&wme);
    return FALSE;
  }

  // Mouse scroll wheel.
  static gboolean HandleScroll(GtkWidget* widget,
                               GdkEventScroll* event,
                               WebWidgetHost* host) {
    const WebMouseWheelEvent& wmwe =
        WebInputEventFactory::mouseWheelEvent(event);
    host->webwidget()->HandleInputEvent(&wmwe);
    return FALSE;
  }

  DISALLOW_IMPLICIT_CONSTRUCTORS(WebWidgetHostGtkWidget);
};

}  // namespace

// This is provided so that the webview can reuse the custom GTK window code.
// static
gfx::NativeView WebWidgetHost::CreateWidget(
    gfx::NativeView parent_view, WebWidgetHost* host) {
  return WebWidgetHostGtkWidget::CreateNewWidget(parent_view, host);
}

// static
WebWidgetHost* WebWidgetHost::Create(GtkWidget* parent_view,
                                     WebWidgetDelegate* delegate) {
  WebWidgetHost* host = new WebWidgetHost();
  host->view_ = CreateWidget(parent_view, host);
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

  if (!g_handling_expose) {
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
  skia::PlatformDevice& platdev = canvas_->getTopPlatformDevice();
  skia::BitmapPlatformDevice* const bitdev =
      static_cast<skia::BitmapPlatformDevice*>(&platdev);
  cairo_t* cairo_drawable = gdk_cairo_create(window);
  cairo_set_source_surface(cairo_drawable, bitdev->surface(), 0, 0);
  cairo_paint(cairo_drawable);
  cairo_destroy(cairo_drawable);

  gdk_window_end_paint(window);
}

WebScreenInfo WebWidgetHost::GetScreenInfo() {
  Display* display = test_shell_x11::GtkWidgetGetDisplay(view_);
  int screen_num = test_shell_x11::GtkWidgetGetScreenNum(view_);
  return WebScreenInfoFactory::screenInfo(display, screen_num);
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
