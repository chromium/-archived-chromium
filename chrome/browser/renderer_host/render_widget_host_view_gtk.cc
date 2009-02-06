// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_host/render_widget_host_view_gtk.h"

#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <cairo/cairo.h>

#include "base/logging.h"
#include "chrome/browser/renderer_host/backing_store.h"
#include "chrome/browser/renderer_host/render_widget_host.h"
#include "skia/ext/bitmap_platform_device_linux.h"
#include "skia/ext/platform_device_linux.h"

namespace {

// -----------------------------------------------------------------------------
// Callback functions to proxy to RenderWidgetHostViewGtk...

gboolean ConfigureEvent(GtkWidget* widget, GdkEventConfigure* config,
                        RenderWidgetHostViewGtk* host) {
  NOTIMPLEMENTED();
  return FALSE;
}

gboolean ExposeEvent(GtkWidget* widget, GdkEventExpose* expose,
                     RenderWidgetHostViewGtk* host) {
  const gfx::Rect damage_rect(expose->area);
  host->Paint(damage_rect);
  return FALSE;
}

gboolean KeyPressReleaseEvent(GtkWidget* widget, GdkEventKey* event,
                              RenderWidgetHostViewGtk* host) {
  NOTIMPLEMENTED();
  return FALSE;
}

gboolean FocusIn(GtkWidget* widget, GdkEventFocus* focus,
                 RenderWidgetHostViewGtk* host) {
  NOTIMPLEMENTED();
  return FALSE;
}

gboolean FocusOut(GtkWidget* widget, GdkEventFocus* focus,
                  RenderWidgetHostViewGtk* host) {
  NOTIMPLEMENTED();
  return FALSE;
}

gboolean ButtonPressReleaseEvent(GtkWidget* widget, GdkEventButton* event,
                                 RenderWidgetHostViewGtk* host) {
  NOTIMPLEMENTED();
  return FALSE;
}

gboolean MouseMoveEvent(GtkWidget* widget, GdkEventMotion* event,
                        RenderWidgetHostViewGtk* host) {
  NOTIMPLEMENTED();
  return FALSE;
}

gboolean MouseScrollEvent(GtkWidget* widget, GdkEventScroll* event,
                          RenderWidgetHostViewGtk* host) {
  NOTIMPLEMENTED();
  return FALSE;
}

}  // anonymous namespace

// static
RenderWidgetHostView* RenderWidgetHostView::CreateViewForWidget(
    RenderWidgetHost* widget) {
  return new RenderWidgetHostViewGtk(widget);
}

RenderWidgetHostViewGtk::RenderWidgetHostViewGtk(RenderWidgetHost* widget)
    : widget_(widget) {
  widget_->set_view(this);

  view_ = gtk_drawing_area_new();

  gtk_widget_add_events(view_, GDK_EXPOSURE_MASK |
                               GDK_POINTER_MOTION_MASK |
                               GDK_BUTTON_PRESS_MASK |
                               GDK_BUTTON_RELEASE_MASK |
                               GDK_KEY_PRESS_MASK |
                               GDK_KEY_RELEASE_MASK);
  GTK_WIDGET_SET_FLAGS(view_, GTK_CAN_FOCUS);
  g_signal_connect(view_, "configure-event", G_CALLBACK(ConfigureEvent), this);
  g_signal_connect(view_, "expose-event", G_CALLBACK(ExposeEvent), this);
  g_signal_connect(view_, "key-press-event", G_CALLBACK(KeyPressReleaseEvent),
                   this);
  g_signal_connect(view_, "key-release-event",
                   G_CALLBACK(KeyPressReleaseEvent), this);
  g_signal_connect(view_, "focus-in-event", G_CALLBACK(FocusIn), this);
  g_signal_connect(view_, "focus-out-event", G_CALLBACK(FocusOut), this);
  g_signal_connect(view_, "button-press-event",
                   G_CALLBACK(ButtonPressReleaseEvent), this);
  g_signal_connect(view_, "button-release-event",
                   G_CALLBACK(ButtonPressReleaseEvent), this);
  g_signal_connect(view_, "motion-notify-event", G_CALLBACK(MouseMoveEvent),
                   this);
  g_signal_connect(view_, "scroll-event", G_CALLBACK(MouseScrollEvent),
                   this);
}

RenderWidgetHostViewGtk::~RenderWidgetHostViewGtk() {
  gtk_widget_destroy(view_);
}

void RenderWidgetHostViewGtk::DidBecomeSelected() {
  NOTIMPLEMENTED();
}

void RenderWidgetHostViewGtk::WasHidden() {
  NOTIMPLEMENTED();
}

void RenderWidgetHostViewGtk::SetSize(const gfx::Size& size) {
  NOTIMPLEMENTED();
}

gfx::NativeView RenderWidgetHostViewGtk::GetPluginNativeView() {
  NOTIMPLEMENTED();
  return NULL;
}

void RenderWidgetHostViewGtk::MovePluginWindows(
    const std::vector<WebPluginGeometry>& plugin_window_moves) {
  NOTIMPLEMENTED();
}

void RenderWidgetHostViewGtk::Focus() {
  NOTIMPLEMENTED();
}

void RenderWidgetHostViewGtk::Blur() {
  NOTIMPLEMENTED();
}

bool RenderWidgetHostViewGtk::HasFocus() {
  NOTIMPLEMENTED();
  return false;
}

void RenderWidgetHostViewGtk::Show() {
  NOTIMPLEMENTED();
}

void RenderWidgetHostViewGtk::Hide() {
  NOTIMPLEMENTED();
}

gfx::Rect RenderWidgetHostViewGtk::GetViewBounds() const {
  NOTIMPLEMENTED();
  return gfx::Rect();
}

void RenderWidgetHostViewGtk::UpdateCursor(const WebCursor& cursor) {
  NOTIMPLEMENTED();
}

void RenderWidgetHostViewGtk::UpdateCursorIfOverSelf() {
  NOTIMPLEMENTED();
}

void RenderWidgetHostViewGtk::SetIsLoading(bool is_loading) {
  NOTIMPLEMENTED();
}

void RenderWidgetHostViewGtk::IMEUpdateStatus(int control,
                                              const gfx::Rect& caret_rect) {
  NOTIMPLEMENTED();
}

void RenderWidgetHostViewGtk::DidPaintRect(const gfx::Rect& rect) {
  NOTIMPLEMENTED();
}

void RenderWidgetHostViewGtk::DidScrollRect(const gfx::Rect& rect, int dx,
                                            int dy) {
  NOTIMPLEMENTED();
}

void RenderWidgetHostViewGtk::RendererGone() {
  NOTIMPLEMENTED();
}

void RenderWidgetHostViewGtk::Destroy() {
  NOTIMPLEMENTED();
}

void RenderWidgetHostViewGtk::SetTooltipText(const std::wstring& tooltip_text) {
  NOTIMPLEMENTED();
}

void RenderWidgetHostViewGtk::Paint(const gfx::Rect& damage_rect) {
  BackingStore* backing_store = widget_->GetBackingStore();

  if (backing_store) {
    GdkRectangle grect = {
      damage_rect.x(),
      damage_rect.y(),
      damage_rect.width(),
      damage_rect.height()
    };
    GdkWindow* window = view_->window;
    gdk_window_begin_paint_rect(window, &grect);

    skia::PlatformDeviceLinux &platdev =
        backing_store->canvas()->getTopPlatformDevice();
    skia::BitmapPlatformDeviceLinux* const bitdev =
        static_cast<skia::BitmapPlatformDeviceLinux* >(&platdev);
    cairo_t* cairo_drawable = gdk_cairo_create(window);
    cairo_set_source_surface(cairo_drawable, bitdev->surface(), 0, 0);
    cairo_paint(cairo_drawable);
    cairo_destroy(cairo_drawable);

    gdk_window_end_paint(window);
  } else {
    NOTIMPLEMENTED();
  }
}

