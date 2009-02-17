// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_host/render_widget_host_view_gtk.h"

#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <cairo/cairo.h>

#include "base/logging.h"
#include "base/string_util.h"
#include "chrome/browser/renderer_host/backing_store.h"
#include "chrome/browser/renderer_host/render_widget_host.h"
#include "skia/ext/bitmap_platform_device_linux.h"
#include "skia/ext/platform_device_linux.h"
#include "webkit/glue/webinputevent.h"

namespace {

// This class is a simple convenience wrapper for Gtk functions. It has only
// static methods.
class RenderWidgetHostViewGtkWidget {
 public:
  static GtkWidget* CreateNewWidget(RenderWidgetHostViewGtk* host_view) {
    GtkWidget* widget = gtk_drawing_area_new();

    gtk_widget_add_events(widget, GDK_EXPOSURE_MASK |
                                  GDK_POINTER_MOTION_MASK |
                                  GDK_BUTTON_PRESS_MASK |
                                  GDK_BUTTON_RELEASE_MASK |
                                  GDK_KEY_PRESS_MASK |
                                  GDK_KEY_RELEASE_MASK);
    GTK_WIDGET_SET_FLAGS(widget, GTK_CAN_FOCUS);

    g_signal_connect(widget, "configure-event",
                     G_CALLBACK(ConfigureEvent), host_view);
    g_signal_connect(widget, "expose-event",
                     G_CALLBACK(ExposeEvent), host_view);
    g_signal_connect(widget, "key-press-event",
                     G_CALLBACK(KeyPressReleaseEvent), host_view);
    g_signal_connect(widget, "key-release-event",
                     G_CALLBACK(KeyPressReleaseEvent), host_view);
    g_signal_connect(widget, "focus",
                     G_CALLBACK(Focus), host_view);
    g_signal_connect(widget, "focus-in-event",
                     G_CALLBACK(FocusIn), host_view);
    g_signal_connect(widget, "focus-out-event",
                     G_CALLBACK(FocusOut), host_view);
    g_signal_connect(widget, "button-press-event",
                     G_CALLBACK(ButtonPressReleaseEvent), host_view);
    g_signal_connect(widget, "button-release-event",
                     G_CALLBACK(ButtonPressReleaseEvent), host_view);
    g_signal_connect(widget, "motion-notify-event",
                     G_CALLBACK(MouseMoveEvent), host_view);
    g_signal_connect(widget, "scroll-event",
                     G_CALLBACK(MouseScrollEvent), host_view);

    return widget;
  }

 private:
  static gboolean ConfigureEvent(GtkWidget* widget, GdkEventConfigure* config,
                                 RenderWidgetHostViewGtk* host_view) {
    host_view->GetRenderWidgetHost()->WasResized();
    return FALSE;
  }

  static gboolean ExposeEvent(GtkWidget* widget, GdkEventExpose* expose,
                              RenderWidgetHostViewGtk* host_view) {
    const gfx::Rect damage_rect(expose->area);
    host_view->Paint(damage_rect);
    return FALSE;
  }

  static gboolean KeyPressReleaseEvent(GtkWidget* widget, GdkEventKey* event,
                                       RenderWidgetHostViewGtk* host_view) {
    WebKeyboardEvent wke(event);
    host_view->GetRenderWidgetHost()->ForwardKeyboardEvent(wke);

    // See note in webwidget_host_gtk.cc::HandleKeyPress().
    if (event->type == GDK_KEY_PRESS) {
      wke.type = WebKeyboardEvent::CHAR;
      host_view->GetRenderWidgetHost()->ForwardKeyboardEvent(wke);
    }

    return FALSE;
  }

  static gboolean Focus(GtkWidget* widget, GtkDirectionType focus,
                        RenderWidgetHostViewGtk* host_view) {
    // We override this so that pressing tab navigates within the web contents
    // rather than tabbing out of it.  However, we do want to be able to tab
    // out of it at the appropriate points.  TODO(port): study how this works
    // on Windows and implement it.
    NOTIMPLEMENTED();
    return TRUE;
  }

  static gboolean FocusIn(GtkWidget* widget, GdkEventFocus* focus,
                          RenderWidgetHostViewGtk* host_view) {
    host_view->GetRenderWidgetHost()->Focus();
    return FALSE;
  }

  static gboolean FocusOut(GtkWidget* widget, GdkEventFocus* focus,
                           RenderWidgetHostViewGtk* host_view) {
    host_view->GetRenderWidgetHost()->Blur();
    return FALSE;
  }

  static gboolean ButtonPressReleaseEvent(
      GtkWidget* widget, GdkEventButton* event,
      RenderWidgetHostViewGtk* host_view) {
    WebMouseEvent wme(event);
    host_view->GetRenderWidgetHost()->ForwardMouseEvent(wme);

    // TODO(evanm): why is this necessary here but not in test shell?
    // This logic is the same as GtkButton.
    if (event->type == GDK_BUTTON_PRESS && !GTK_WIDGET_HAS_FOCUS(widget))
      gtk_widget_grab_focus(widget);

    return FALSE;
  }

  static gboolean MouseMoveEvent(GtkWidget* widget, GdkEventMotion* event,
                                 RenderWidgetHostViewGtk* host_view) {
    WebMouseEvent wme(event);
    host_view->GetRenderWidgetHost()->ForwardMouseEvent(wme);
    return FALSE;
  }

  static gboolean MouseScrollEvent(GtkWidget* widget, GdkEventScroll* event,
                                   RenderWidgetHostViewGtk* host_view) {
    WebMouseWheelEvent wmwe(event);
    host_view->GetRenderWidgetHost()->ForwardWheelEvent(wmwe);
    return FALSE;
  }

  DISALLOW_IMPLICIT_CONSTRUCTORS(RenderWidgetHostViewGtkWidget);
};

}  // namespace

// static
RenderWidgetHostView* RenderWidgetHostView::CreateViewForWidget(
    RenderWidgetHost* widget) {
  return new RenderWidgetHostViewGtk(widget);
}

RenderWidgetHostViewGtk::RenderWidgetHostViewGtk(RenderWidgetHost* widget_host)
    : host_(widget_host) {
  host_->set_view(this);
  view_ = RenderWidgetHostViewGtkWidget::CreateNewWidget(this);
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
  // TODO(port): We need to pass some widget pointer out here because the
  // renderer echos it back to us when it asks for GetScreenInfo. However, we
  // should probably be passing the top-level window or some such instead.
  return view_;
}

void RenderWidgetHostViewGtk::MovePluginWindows(
    const std::vector<WebPluginGeometry>& plugin_window_moves) {
  if (plugin_window_moves.empty())
    return;

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
  return gfx::Rect(view_->allocation.x, view_->allocation.y,
                   view_->allocation.width, view_->allocation.height);
}

void RenderWidgetHostViewGtk::UpdateCursor(const WebCursor& cursor) {
  // TODO(port): some of this logic may need moving to UpdateCursorIfOverSelf at
  // some point.
  GdkCursorType current_cursor_type = current_cursor_.GetCursorType();
  GdkCursorType new_cursor_type = cursor.GetCursorType();
  current_cursor_ = cursor;
  GdkCursor* gdk_cursor;
  if (new_cursor_type == GDK_CURSOR_IS_PIXMAP) {
    // TODO(port): WebKit bug https://bugs.webkit.org/show_bug.cgi?id=16388 is
    // that calling gdk_window_set_cursor repeatedly is expensive.  We should
    // avoid it here where possible.
    gdk_cursor = current_cursor_.GetCustomCursor();
  } else {
    // Optimize the common case, where the cursor hasn't changed.
    // However, we can switch between different pixmaps, so only on the
    // non-pixmap branch.
    if (new_cursor_type == current_cursor_type)
      return;
    gdk_cursor = gdk_cursor_new(new_cursor_type);
  }
  gdk_window_set_cursor(view_->window, gdk_cursor);
  // The window now owns the cursor.
  gdk_cursor_unref(gdk_cursor);
}

void RenderWidgetHostViewGtk::UpdateCursorIfOverSelf() {
  // Windows uses this to show the resizer arrow if the mouse is over the
  // bottom-right corner.
  NOTIMPLEMENTED();
}

void RenderWidgetHostViewGtk::SetIsLoading(bool is_loading) {
  // Windows tracks loading whether it's loading to switch the cursor
  // out for the arrow+hourglass one.  We don't have such a cursor, so we just
  // ignore this.
}

void RenderWidgetHostViewGtk::IMEUpdateStatus(int control,
                                              const gfx::Rect& caret_rect) {
  NOTIMPLEMENTED();
}

void RenderWidgetHostViewGtk::DidPaintRect(const gfx::Rect& rect) {
  Paint(rect);
}

void RenderWidgetHostViewGtk::DidScrollRect(const gfx::Rect& rect, int dx,
                                            int dy) {
  Paint(rect);
}

void RenderWidgetHostViewGtk::RendererGone() {
  NOTIMPLEMENTED();
}

void RenderWidgetHostViewGtk::Destroy() {
  NOTIMPLEMENTED();
}

void RenderWidgetHostViewGtk::SetTooltipText(const std::wstring& tooltip_text) {
  if (tooltip_text.empty()) {
    gtk_widget_set_has_tooltip(view_, FALSE);
  } else {
    gtk_widget_set_tooltip_text(view_, WideToUTF8(tooltip_text).c_str());
  }
}

void RenderWidgetHostViewGtk::Paint(const gfx::Rect& damage_rect) {
  BackingStore* backing_store = host_->GetBackingStore();

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

