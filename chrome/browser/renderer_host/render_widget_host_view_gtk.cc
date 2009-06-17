// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_host/render_widget_host_view_gtk.h"

#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <gdk/gdkkeysyms.h>
#include <gdk/gdkx.h>
#include <cairo/cairo.h>

#include "base/gfx/gtk_util.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/string_util.h"
#include "base/task.h"
#include "base/time.h"
#include "chrome/common/native_web_keyboard_event.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/x11_util.h"
#include "chrome/browser/renderer_host/backing_store.h"
#include "chrome/browser/renderer_host/render_widget_host.h"
#include "webkit/api/public/gtk/WebInputEventFactory.h"
#include "webkit/glue/webcursor_gtk_data.h"

static const int kMaxWindowWidth = 4000;
static const int kMaxWindowHeight = 4000;

using WebKit::WebInputEventFactory;

// This class is a simple convenience wrapper for Gtk functions. It has only
// static methods.
class RenderWidgetHostViewGtkWidget {
 public:
  static GtkWidget* CreateNewWidget(RenderWidgetHostViewGtk* host_view) {
    GtkWidget* widget = gtk_drawing_area_new();
    gtk_widget_set_double_buffered(widget, FALSE);
#if defined(NDEBUG)
    gtk_widget_modify_bg(widget, GTK_STATE_NORMAL, &gfx::kGdkWhite);
#else
    gtk_widget_modify_bg(widget, GTK_STATE_NORMAL, &gfx::kGdkGreen);
#endif

    gtk_widget_add_events(widget, GDK_EXPOSURE_MASK |
                                  GDK_POINTER_MOTION_MASK |
                                  GDK_BUTTON_PRESS_MASK |
                                  GDK_BUTTON_RELEASE_MASK |
                                  GDK_KEY_PRESS_MASK |
                                  GDK_KEY_RELEASE_MASK);
    GTK_WIDGET_SET_FLAGS(widget, GTK_CAN_FOCUS);

    g_signal_connect(widget, "size-allocate",
                     G_CALLBACK(SizeAllocate), host_view);
    g_signal_connect(widget, "expose-event",
                     G_CALLBACK(ExposeEvent), host_view);
    g_signal_connect(widget, "key-press-event",
                     G_CALLBACK(KeyPressReleaseEvent), host_view);
    g_signal_connect(widget, "key-release-event",
                     G_CALLBACK(KeyPressReleaseEvent), host_view);
    g_signal_connect(widget, "focus-in-event",
                     G_CALLBACK(OnFocusIn), host_view);
    g_signal_connect(widget, "focus-out-event",
                     G_CALLBACK(OnFocusOut), host_view);
    g_signal_connect(widget, "button-press-event",
                     G_CALLBACK(ButtonPressReleaseEvent), host_view);
    g_signal_connect(widget, "button-release-event",
                     G_CALLBACK(ButtonPressReleaseEvent), host_view);
    g_signal_connect(widget, "motion-notify-event",
                     G_CALLBACK(MouseMoveEvent), host_view);
    g_signal_connect(widget, "scroll-event",
                     G_CALLBACK(MouseScrollEvent), host_view);

    GtkTargetList* target_list = gtk_target_list_new(NULL, 0);
    gtk_target_list_add_text_targets(target_list, 0);
    gint num_targets = 0;
    GtkTargetEntry* targets = gtk_target_table_new_from_list(target_list,
                                                             &num_targets);
    gtk_selection_clear_targets(widget, GDK_SELECTION_PRIMARY);
    gtk_selection_add_targets(widget, GDK_SELECTION_PRIMARY, targets,
                              num_targets);
    gtk_target_list_unref(target_list);
    gtk_target_table_free(targets, num_targets);
    return widget;
  }

 private:
  static gboolean SizeAllocate(GtkWidget* widget, GtkAllocation* allocation,
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
    if (host_view->parent_ && host_view->activatable() &&
        GDK_Escape == event->keyval) {
      // Force popups to close on Esc just in case the renderer is hung.  This
      // allows us to release our keyboard grab.
      host_view->host_->Shutdown();
    } else {
      NativeWebKeyboardEvent wke(event);
      host_view->GetRenderWidgetHost()->ForwardKeyboardEvent(wke);
    }
    // We return TRUE because we did handle the event. If it turns out webkit
    // can't handle the event, we'll deal with it in
    // RenderView::UnhandledKeyboardEvent().
    return TRUE;
  }

  static gboolean OnFocusIn(GtkWidget* widget, GdkEventFocus* focus,
                            RenderWidgetHostViewGtk* host_view) {
    int x, y;
    gtk_widget_get_pointer(widget, &x, &y);
    // http://crbug.com/13389
    // If the cursor is in the render view, fake a mouse move event so that
    // webkit updates its state. Otherwise webkit might think the cursor is
    // somewhere it's not.
    if (x >= 0 && y >= 0 && x < widget->allocation.width &&
        y < widget->allocation.height) {
      WebKit::WebMouseEvent fake_event;
      fake_event.timeStampSeconds = base::Time::Now().ToDoubleT();
      fake_event.modifiers = 0;
      fake_event.windowX = fake_event.x = x;
      fake_event.windowY = fake_event.y = y;
      gdk_window_get_origin(widget->window, &x, &y);
      fake_event.globalX = fake_event.x + x;
      fake_event.globalY = fake_event.y + y;
      fake_event.type = WebKit::WebInputEvent::MouseMove;
      fake_event.button = WebKit::WebMouseEvent::ButtonNone;
      host_view->GetRenderWidgetHost()->ForwardMouseEvent(fake_event);
    }

    host_view->ShowCurrentCursor();
    host_view->GetRenderWidgetHost()->Focus();
    return FALSE;
  }

  static gboolean OnFocusOut(GtkWidget* widget, GdkEventFocus* focus,
                             RenderWidgetHostViewGtk* host_view) {
    // Whenever we lose focus, set the cursor back to that of our parent window,
    // which should be the default arrow.
    gdk_window_set_cursor(widget->window, NULL);
    // If we are showing a context menu, maintain the illusion that webkit has
    // focus.
    if (!host_view->is_showing_context_menu_)
      host_view->GetRenderWidgetHost()->Blur();
    return FALSE;
  }

  static gboolean ButtonPressReleaseEvent(
      GtkWidget* widget, GdkEventButton* event,
      RenderWidgetHostViewGtk* host_view) {
    // We want to translate the coordinates of events that do not originate
    // from this widget to be relative to the top left of the widget.
    GtkWidget* event_widget = gtk_get_event_widget(
        reinterpret_cast<GdkEvent*>(event));
    if (event_widget != widget) {
      int x = 0;
      int y = 0;
      gtk_widget_get_pointer(widget, &x, &y);
      // If the mouse release happens outside our popup, force the popup to
      // close.  We do this so a hung renderer doesn't prevent us from
      // releasing the x pointer grab.
      bool click_in_popup = x >= 0 && y >= 0 && x < widget->allocation.width &&
          y < widget->allocation.height;
      if (!host_view->is_popup_first_mouse_release_ && !click_in_popup) {
        host_view->host_->Shutdown();
        return FALSE;
      }
      event->x = x;
      event->y = y;
    }
    host_view->is_popup_first_mouse_release_ = false;
    host_view->GetRenderWidgetHost()->ForwardMouseEvent(
        WebInputEventFactory::mouseEvent(event));

    // TODO(evanm): why is this necessary here but not in test shell?
    // This logic is the same as GtkButton.
    if (event->type == GDK_BUTTON_PRESS && !GTK_WIDGET_HAS_FOCUS(widget))
      gtk_widget_grab_focus(widget);

    return FALSE;
  }

  static gboolean MouseMoveEvent(GtkWidget* widget, GdkEventMotion* event,
                                 RenderWidgetHostViewGtk* host_view) {
    // We want to translate the coordinates of events that do not originate
    // from this widget to be relative to the top left of the widget.
    GtkWidget* event_widget = gtk_get_event_widget(
        reinterpret_cast<GdkEvent*>(event));
    if (event_widget != widget) {
      int x = 0;
      int y = 0;
      gtk_widget_get_pointer(widget, &x, &y);
      event->x = x;
      event->y = y;
    }
    host_view->GetRenderWidgetHost()->ForwardMouseEvent(
        WebInputEventFactory::mouseEvent(event));
    return FALSE;
  }

  static gboolean MouseScrollEvent(GtkWidget* widget, GdkEventScroll* event,
                                   RenderWidgetHostViewGtk* host_view) {
    host_view->GetRenderWidgetHost()->ForwardWheelEvent(
        WebInputEventFactory::mouseWheelEvent(event));
    return FALSE;
  }

  DISALLOW_IMPLICIT_CONSTRUCTORS(RenderWidgetHostViewGtkWidget);
};

// static
RenderWidgetHostView* RenderWidgetHostView::CreateViewForWidget(
    RenderWidgetHost* widget) {
  return new RenderWidgetHostViewGtk(widget);
}

RenderWidgetHostViewGtk::RenderWidgetHostViewGtk(RenderWidgetHost* widget_host)
    : host_(widget_host),
      about_to_validate_and_paint_(false),
      is_hidden_(false),
      is_loading_(false),
      is_showing_context_menu_(false),
      parent_host_view_(NULL),
      parent_(NULL),
      is_popup_first_mouse_release_(true) {
  host_->set_view(this);
}

RenderWidgetHostViewGtk::~RenderWidgetHostViewGtk() {
  view_.Destroy();
}

void RenderWidgetHostViewGtk::InitAsChild() {
  view_.Own(RenderWidgetHostViewGtkWidget::CreateNewWidget(this));
  gtk_widget_show(view_.get());
}

void RenderWidgetHostViewGtk::InitAsPopup(
    RenderWidgetHostView* parent_host_view, const gfx::Rect& pos) {
  parent_host_view_ = parent_host_view;
  parent_ = parent_host_view->GetNativeView();
  GtkWidget* popup = gtk_window_new(GTK_WINDOW_POPUP);
  view_.Own(RenderWidgetHostViewGtkWidget::CreateNewWidget(this));
  gtk_container_add(GTK_CONTAINER(popup), view_.get());

  // If we are not activatable, we don't want to grab keyboard input,
  // and webkit will manage our destruction.
  if (activatable()) {
    // Grab all input for the app. If a click lands outside the bounds of the
    // popup, WebKit will notice and destroy us.
    gtk_grab_add(view_.get());
    // Now grab all of X's input.
    gdk_pointer_grab(
        parent_->window,
        TRUE,  // Only events outside of the window are reported with respect
               // to |parent_->window|.
        static_cast<GdkEventMask>(GDK_BUTTON_PRESS_MASK |
            GDK_BUTTON_RELEASE_MASK | GDK_POINTER_MOTION_MASK),
        NULL,
        NULL,
        GDK_CURRENT_TIME);
    // We grab keyboard events too so things like alt+tab are eaten.
    gdk_keyboard_grab(parent_->window, TRUE, GDK_CURRENT_TIME);

    // Our parent widget actually keeps GTK focus within its window, but we have
    // to make the webkit selection box disappear to maintain appearances.
    parent_host_view->Blur();
  }

  gtk_widget_set_size_request(view_.get(),
                              std::min(pos.width(), kMaxWindowWidth),
                              std::min(pos.height(), kMaxWindowHeight));

  gtk_window_set_default_size(GTK_WINDOW(popup), -1, -1);
  // Don't allow the window to be resized. This also forces the window to
  // shrink down to the size of its child contents.
  gtk_window_set_resizable(GTK_WINDOW(popup), FALSE);
  gtk_window_move(GTK_WINDOW(popup), pos.x(), pos.y());
  gtk_widget_show_all(popup);
}

void RenderWidgetHostViewGtk::DidBecomeSelected() {
  if (!is_hidden_)
    return;

  is_hidden_ = false;
  host_->WasRestored();
}

void RenderWidgetHostViewGtk::WasHidden() {
  if (is_hidden_)
    return;

  // If we receive any more paint messages while we are hidden, we want to
  // ignore them so we don't re-allocate the backing store.  We will paint
  // everything again when we become selected again.
  is_hidden_ = true;

  // If we have a renderer, then inform it that we are being hidden so it can
  // reduce its resource utilization.
  GetRenderWidgetHost()->WasHidden();
}

void RenderWidgetHostViewGtk::SetSize(const gfx::Size& size) {
  // This is called when webkit has sent us a Move message.
  // If we are a popup, we want to handle this.
  // TODO(estade): are there other situations where we want to respect the
  // request?
  if (parent_) {
    gtk_widget_set_size_request(view_.get(),
                                std::min(size.width(), kMaxWindowWidth),
                                std::min(size.height(), kMaxWindowHeight));
  }
}

gfx::NativeView RenderWidgetHostViewGtk::GetNativeView() {
  return view_.get();
}

void RenderWidgetHostViewGtk::MovePluginWindows(
    const std::vector<WebPluginGeometry>& plugin_window_moves) {
  if (plugin_window_moves.empty())
    return;

  NOTIMPLEMENTED();
}

void RenderWidgetHostViewGtk::Focus() {
  gtk_widget_grab_focus(view_.get());
}

void RenderWidgetHostViewGtk::Blur() {
  // TODO(estade): We should be clearing native focus as well, but I know of no
  // way to do that without focusing another widget.
  // TODO(estade): it doesn't seem like the CanBlur() check should be necessary
  // since we are only called in response to a ViewHost blur message, but this
  // check is made on Windows so I've added it here as well.
  if (host_->CanBlur())
    host_->Blur();
}

bool RenderWidgetHostViewGtk::HasFocus() {
  return gtk_widget_is_focus(view_.get());
}

void RenderWidgetHostViewGtk::Show() {
  gtk_widget_show(view_.get());
}

void RenderWidgetHostViewGtk::Hide() {
  gtk_widget_hide(view_.get());
}

gfx::Rect RenderWidgetHostViewGtk::GetViewBounds() const {
  GtkAllocation* alloc = &view_.get()->allocation;
  return gfx::Rect(alloc->x, alloc->y, alloc->width, alloc->height);
}

void RenderWidgetHostViewGtk::UpdateCursor(const WebCursor& cursor) {
  // Optimize the common case, where the cursor hasn't changed.
  // However, we can switch between different pixmaps, so only on the
  // non-pixmap branch.
  if (current_cursor_.GetCursorType() != GDK_CURSOR_IS_PIXMAP &&
      current_cursor_.GetCursorType() == cursor.GetCursorType())
    return;

  current_cursor_ = cursor;
  ShowCurrentCursor();
}

void RenderWidgetHostViewGtk::SetIsLoading(bool is_loading) {
  is_loading_ = is_loading;
  // Only call ShowCurrentCursor() when it will actually change the cursor.
  if (current_cursor_.GetCursorType() == GDK_LAST_CURSOR)
    ShowCurrentCursor();
}

void RenderWidgetHostViewGtk::IMEUpdateStatus(int control,
                                              const gfx::Rect& caret_rect) {
  NOTIMPLEMENTED();
}

void RenderWidgetHostViewGtk::DidPaintRect(const gfx::Rect& rect) {
  if (is_hidden_)
    return;

  if (about_to_validate_and_paint_)
    invalid_rect_ = invalid_rect_.Union(rect);
  else
    Paint(rect);
}

void RenderWidgetHostViewGtk::DidScrollRect(const gfx::Rect& rect, int dx,
                                            int dy) {
  if (is_hidden_)
    return;

  Paint(rect);
}

void RenderWidgetHostViewGtk::RenderViewGone() {
  Destroy();
}

void RenderWidgetHostViewGtk::Destroy() {
  // If |parent_| is non-null, we are a popup and we must disconnect from our
  // parent and destroy the popup window.
  if (parent_) {
    if (activatable()) {
      GdkDisplay *display = gtk_widget_get_display(parent_);
      gdk_display_pointer_ungrab(display, GDK_CURRENT_TIME);
      gdk_display_keyboard_ungrab(display, GDK_CURRENT_TIME);
      parent_host_view_->Focus();
    }
    gtk_widget_destroy(gtk_widget_get_parent(view_.get()));
  }

  // Remove |view_| from all containers now, so nothing else can hold a
  // reference to |view_|'s widget except possibly a gtk signal handler if
  // this code is currently executing within the context of a gtk signal
  // handler.  Note that |view_| is still alive after this call.  It will be
  // deallocated in the destructor.
  // See http://www.crbug.com/11847 for details.
  gtk_widget_destroy(view_.get());

  MessageLoop::current()->DeleteSoon(FROM_HERE, this);
}

void RenderWidgetHostViewGtk::SetTooltipText(const std::wstring& tooltip_text) {
  if (tooltip_text.empty()) {
    gtk_widget_set_has_tooltip(view_.get(), FALSE);
  } else {
    gtk_widget_set_tooltip_text(view_.get(), WideToUTF8(tooltip_text).c_str());
  }
}

void RenderWidgetHostViewGtk::SelectionChanged(const std::string& text) {
  GtkClipboard* x_clipboard = gtk_clipboard_get(GDK_SELECTION_PRIMARY);
  gtk_clipboard_set_text(x_clipboard, text.c_str(), text.length());
}

void RenderWidgetHostViewGtk::PasteFromSelectionClipboard() {
  GtkClipboard* x_clipboard = gtk_clipboard_get(GDK_SELECTION_PRIMARY);
  gtk_clipboard_request_text(x_clipboard, ReceivedSelectionText, this);
}

void RenderWidgetHostViewGtk::ShowingContextMenu(bool showing) {
  is_showing_context_menu_ = showing;
}

BackingStore* RenderWidgetHostViewGtk::AllocBackingStore(
    const gfx::Size& size) {
  return new BackingStore(host_, size,
                          x11_util::GetVisualFromGtkWidget(view_.get()),
                          gtk_widget_get_visual(view_.get())->depth);
}

void RenderWidgetHostViewGtk::Paint(const gfx::Rect& damage_rect) {
  DCHECK(!about_to_validate_and_paint_);

  invalid_rect_ = damage_rect;
  about_to_validate_and_paint_ = true;
  BackingStore* backing_store = host_->GetBackingStore(true);
  // Calling GetBackingStore maybe have changed |invalid_rect_|...
  about_to_validate_and_paint_ = false;

  gfx::Rect paint_rect = gfx::Rect(0, 0, kMaxWindowWidth, kMaxWindowHeight);
  paint_rect = paint_rect.Intersect(invalid_rect_);

  GdkWindow* window = view_.get()->window;
  if (backing_store) {
    // Only render the widget if it is attached to a window; there's a short
    // period where this object isn't attached to a window but hasn't been
    // Destroy()ed yet and it receives paint messages...
    if (window) {
      backing_store->ShowRect(
          paint_rect, x11_util::GetX11WindowFromGtkWidget(view_.get()));
    }
  } else {
    if (window)
      gdk_window_clear(window);
  }
}

void RenderWidgetHostViewGtk::ShowCurrentCursor() {
  // The widget may not have a window. If that's the case, abort mission. This
  // is the same issue as that explained above in Paint().
  if (!view_.get()->window)
    return;

  GdkCursor* gdk_cursor;
  switch (current_cursor_.GetCursorType()) {
    case GDK_CURSOR_IS_PIXMAP:
      // TODO(port): WebKit bug https://bugs.webkit.org/show_bug.cgi?id=16388 is
      // that calling gdk_window_set_cursor repeatedly is expensive.  We should
      // avoid it here where possible.
      gdk_cursor = current_cursor_.GetCustomCursor();
      break;

    case GDK_LAST_CURSOR:
      if (is_loading_) {
        // Use MOZ_CURSOR_SPINNING if we are showing the default cursor and
        // the page is loading.
        static const GdkColor fg = { 0, 0, 0, 0 };
        static const GdkColor bg = { 65535, 65535, 65535, 65535 };
        GdkPixmap* source =
            gdk_bitmap_create_from_data(NULL, moz_spinning_bits, 32, 32);
        GdkPixmap* mask =
            gdk_bitmap_create_from_data(NULL, moz_spinning_mask_bits, 32, 32);
        gdk_cursor = gdk_cursor_new_from_pixmap(source, mask, &fg, &bg, 2, 2);
        g_object_unref(source);
        g_object_unref(mask);
      } else {
        gdk_cursor = NULL;
      }
      break;

    default:
      gdk_cursor = gdk_cursor_new(current_cursor_.GetCursorType());
  }
  gdk_window_set_cursor(view_.get()->window, gdk_cursor);
  // The window now owns the cursor.
  if (gdk_cursor)
    gdk_cursor_unref(gdk_cursor);
}

void RenderWidgetHostViewGtk::ReceivedSelectionText(GtkClipboard* clipboard,
    const gchar* text, gpointer userdata) {
  // If there's nothing to paste (|text| is NULL), do nothing.
  if (!text)
    return;
  RenderWidgetHostViewGtk* host_view =
      reinterpret_cast<RenderWidgetHostViewGtk*>(userdata);
  host_view->host_->Send(new ViewMsg_InsertText(host_view->host_->routing_id(),
                                                UTF8ToUTF16(text)));
}
