// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/window/window_gtk.h"

#include "app/gfx/path.h"
#include "app/l10n_util.h"
#include "base/gfx/rect.h"
#include "views/widget/root_view.h"
#include "views/window/custom_frame_view.h"
#include "views/window/hit_test.h"
#include "views/window/non_client_view.h"
#include "views/window/window_delegate.h"

namespace {

// Converts a Windows-style hit test result code into a GDK window edge.
GdkWindowEdge HitTestCodeToGDKWindowEdge(int hittest_code) {
  switch (hittest_code) {
    case HTBOTTOM:
      return GDK_WINDOW_EDGE_SOUTH;
    case HTBOTTOMLEFT:
      return GDK_WINDOW_EDGE_SOUTH_WEST;
    case HTBOTTOMRIGHT:
    case HTGROWBOX:
      return GDK_WINDOW_EDGE_SOUTH_EAST;
    case HTLEFT:
      return GDK_WINDOW_EDGE_WEST;
    case HTRIGHT:
      return GDK_WINDOW_EDGE_EAST;
    case HTTOP:
      return GDK_WINDOW_EDGE_NORTH;
    case HTTOPLEFT:
      return GDK_WINDOW_EDGE_NORTH_WEST;
    case HTTOPRIGHT:
      return GDK_WINDOW_EDGE_NORTH_EAST;
    default:
      NOTREACHED();
      break;
  }
  // Default to something defaultish.
  return HitTestCodeToGDKWindowEdge(HTGROWBOX);
}

// Converts a Windows-style hit test result code into a GDK cursor type.
GdkCursorType HitTestCodeToGdkCursorType(int hittest_code) {
  switch (hittest_code) {
    case HTBOTTOM:
      return GDK_BOTTOM_SIDE;
    case HTBOTTOMLEFT:
      return GDK_BOTTOM_LEFT_CORNER;
    case HTBOTTOMRIGHT:
    case HTGROWBOX:
      return GDK_BOTTOM_RIGHT_CORNER;
    case HTLEFT:
      return GDK_LEFT_SIDE;
    case HTRIGHT:
      return GDK_RIGHT_SIDE;
    case HTTOP:
      return GDK_TOP_SIDE;
    case HTTOPLEFT:
      return GDK_TOP_LEFT_CORNER;
    case HTTOPRIGHT:
      return GDK_TOP_RIGHT_CORNER;
    default:
      break;
  }
  // Default to something defaultish.
  return GDK_ARROW;
}

}  // namespace

namespace views {

WindowGtk::~WindowGtk() {
}

// static
Window* Window::CreateChromeWindow(gfx::NativeWindow parent,
                                   const gfx::Rect& bounds,
                                   WindowDelegate* window_delegate) {
  WindowGtk* window = new WindowGtk(window_delegate);
  window->GetNonClientView()->SetFrameView(window->CreateFrameViewForWindow());
  window->Init(bounds);
  return window;
}

// static
void Window::CloseAllSecondaryWindows() {
  NOTIMPLEMENTED();
}

gfx::Rect WindowGtk::GetBounds() const {
  gfx::Rect bounds;
  WidgetGtk::GetBounds(&bounds, true);
  return bounds;
}

gfx::Rect WindowGtk::GetNormalBounds() const {
  NOTIMPLEMENTED();
  return GetBounds();
}

void WindowGtk::SetBounds(const gfx::Rect& bounds,
                          gfx::NativeWindow other_window) {
  // TODO: need to deal with other_window.
  WidgetGtk::SetBounds(bounds);
}

void WindowGtk::Show() {
  gtk_widget_show_all(GetNativeView());
}

void WindowGtk::HideWindow() {
  NOTIMPLEMENTED();
}

void WindowGtk::PushForceHidden() {
  NOTIMPLEMENTED();
}

void WindowGtk::PopForceHidden() {
  NOTIMPLEMENTED();
}

void WindowGtk::Activate() {
  NOTIMPLEMENTED();
}

void WindowGtk::Close() {
  if (window_closed_) {
    // Don't do anything if we've already been closed.
    return;
  }

  if (non_client_view_->CanClose()) {
    WidgetGtk::Close();
    window_closed_ = true;
  }
}

void WindowGtk::Maximize() {
  gtk_window_maximize(GetNativeWindow());
}

void WindowGtk::Minimize() {
  gtk_window_iconify(GetNativeWindow());
}

void WindowGtk::Restore() {
  NOTIMPLEMENTED();
}

bool WindowGtk::IsActive() const {
  return gtk_window_is_active(GetNativeWindow());
}

bool WindowGtk::IsVisible() const {
  return GTK_WIDGET_VISIBLE(GetNativeView());
}

bool WindowGtk::IsMaximized() const {
  return window_state_ & GDK_WINDOW_STATE_MAXIMIZED;
}

bool WindowGtk::IsMinimized() const {
  return window_state_ & GDK_WINDOW_STATE_ICONIFIED;
}

void WindowGtk::SetFullscreen(bool fullscreen) {
  if (fullscreen)
    gtk_window_fullscreen(GetNativeWindow());
  else
    gtk_window_unfullscreen(GetNativeWindow());
}

bool WindowGtk::IsFullscreen() const {
  return window_state_ & GDK_WINDOW_STATE_FULLSCREEN;
}

void WindowGtk::EnableClose(bool enable) {
  gtk_window_set_deletable(GetNativeWindow(), enable);
}

void WindowGtk::DisableInactiveRendering() {
  NOTIMPLEMENTED();
}

void WindowGtk::UpdateWindowTitle() {
  // If the non-client view is rendering its own title, it'll need to relayout
  // now.
  non_client_view_->Layout();

  // Update the native frame's text. We do this regardless of whether or not
  // the native frame is being used, since this also updates the taskbar, etc.
  std::wstring window_title = window_delegate_->GetWindowTitle();
  std::wstring localized_text;
  if (l10n_util::AdjustStringForLocaleDirection(window_title, &localized_text))
    window_title.assign(localized_text);

  gtk_window_set_title(GetNativeWindow(), WideToUTF8(window_title).c_str());
}

void WindowGtk::UpdateWindowIcon() {
  NOTIMPLEMENTED();
}

void WindowGtk::SetIsAlwaysOnTop(bool always_on_top) {
  gtk_window_set_keep_above(GetNativeWindow(), always_on_top);
}

NonClientFrameView* WindowGtk::CreateFrameViewForWindow() {
  // TODO(erg): Always use a custom frame view? Are there cases where we let
  // the window manager deal with the X11 equivalent of the "non-client" area?
  return new CustomFrameView(this);
}

void WindowGtk::UpdateFrameAfterFrameChange() {
  NOTIMPLEMENTED();
}

WindowDelegate* WindowGtk::GetDelegate() const {
  return window_delegate_;
}

NonClientView* WindowGtk::GetNonClientView() const {
  return non_client_view_;
}

ClientView* WindowGtk::GetClientView() const {
  return non_client_view_->client_view();
}

gfx::NativeWindow WindowGtk::GetNativeWindow() const {
  return GTK_WINDOW(GetNativeView());
}

bool WindowGtk::ShouldUseNativeFrame() const {
  return false;
}

void WindowGtk::FrameTypeChanged() {
  NOTIMPLEMENTED();
}

////////////////////////////////////////////////////////////////////////////////
// WindowGtk, WidgetGtk overrides:

gboolean WindowGtk::OnButtonPress(GtkWidget* widget, GdkEventButton* event) {
  int hittest_code =
      non_client_view_->NonClientHitTest(gfx::Point(event->x, event->y));
  switch (hittest_code) {
    case HTCAPTION: {
      gfx::Point screen_point(event->x, event->y);
      View::ConvertPointToScreen(GetRootView(), &screen_point);
      gtk_window_begin_move_drag(GetNativeWindow(), event->button,
                                 screen_point.x(), screen_point.y(),
                                 event->time);
      return TRUE;
    }
    case HTBOTTOM:
    case HTBOTTOMLEFT:
    case HTBOTTOMRIGHT:
    case HTGROWBOX:
    case HTLEFT:
    case HTRIGHT:
    case HTTOP:
    case HTTOPLEFT:
    case HTTOPRIGHT: {
      gfx::Point screen_point(event->x, event->y);
      View::ConvertPointToScreen(GetRootView(), &screen_point);
      // TODO(beng): figure out how to get a good minimum size.
      gtk_widget_set_size_request(GetNativeView(), 100, 100);
      gtk_window_begin_resize_drag(GetNativeWindow(),
                                   HitTestCodeToGDKWindowEdge(hittest_code),
                                   event->button, screen_point.x(),
                                   screen_point.y(), event->time);
      return TRUE;
    }
    default:
      // Everything else falls into standard client event handling...
      break;
  }
  return WidgetGtk::OnButtonPress(widget, event);
}

gboolean WindowGtk::OnConfigureEvent(GtkWidget* widget,
                                     GdkEventConfigure* event) {
  SaveWindowPosition();
  return FALSE;
}

gboolean WindowGtk::OnMotionNotify(GtkWidget* widget, GdkEventMotion* event) {
  // Update the cursor for the screen edge.
  int hittest_code =
      non_client_view_->NonClientHitTest(gfx::Point(event->x, event->y));
  GdkCursorType cursor_type = HitTestCodeToGdkCursorType(hittest_code);
  GdkCursor* cursor = gdk_cursor_new(cursor_type);
  gdk_window_set_cursor(widget->window, cursor);
  gdk_cursor_destroy(cursor);

  return WidgetGtk::OnMotionNotify(widget, event);
}

void WindowGtk::OnSizeAllocate(GtkWidget* widget, GtkAllocation* allocation) {
  WidgetGtk::OnSizeAllocate(widget, allocation);

  // The Window's NonClientView may provide a custom shape for the Window.
  gfx::Path window_mask;
  non_client_view_->GetWindowMask(gfx::Size(allocation->width,
                                            allocation->height),
                                  &window_mask);
  GdkRegion* mask_region = window_mask.CreateGdkRegion();
  gdk_window_shape_combine_region(GetNativeView()->window, mask_region, 0, 0);
  gdk_region_destroy(mask_region);
}

gboolean WindowGtk::OnWindowStateEvent(GtkWidget* widget,
                                       GdkEventWindowState* event) {
  window_state_ = event->new_window_state;
  if (!(window_state_ & GDK_WINDOW_STATE_WITHDRAWN))
    SaveWindowPosition();
  return FALSE;
}

////////////////////////////////////////////////////////////////////////////////
// WindowGtk, protected:

WindowGtk::WindowGtk(WindowDelegate* window_delegate)
    : WidgetGtk(TYPE_WINDOW),
      is_modal_(false),
      window_delegate_(window_delegate),
      non_client_view_(new NonClientView(this)),
      window_state_(GDK_WINDOW_STATE_WITHDRAWN),
      window_closed_(false) {
  is_window_ = true;
  window_delegate_->window_.reset(this);
}

void WindowGtk::Init(const gfx::Rect& bounds) {
  // We call this after initializing our members since our implementations of
  // assorted WidgetWin functions may be called during initialization.
  is_modal_ = window_delegate_->IsModal();
  if (is_modal_) {
    // TODO(erg): Fix once modality works.
    // BecomeModal();
  }

  WidgetGtk::Init(NULL, bounds);

  g_signal_connect(G_OBJECT(GetNativeWindow()), "configure-event",
                   G_CALLBACK(CallConfigureEvent), this);
  g_signal_connect(G_OBJECT(GetNativeWindow()), "window-state-event",
                   G_CALLBACK(CallWindowStateEvent), this);

  // Create the ClientView, add it to the NonClientView and add the
  // NonClientView to the RootView. This will cause everything to be parented.
  non_client_view_->set_client_view(window_delegate_->CreateClientView(this));
  WidgetGtk::SetContentsView(non_client_view_);

  UpdateWindowTitle();
  SetInitialBounds(bounds);

  // if (!IsAppWindow()) {
  //   notification_registrar_.Add(
  //       this,
  //       NotificationType::ALL_APPWINDOWS_CLOSED,
  //       NotificationService::AllSources());
  // }

  // ResetWindowRegion(false);
}

////////////////////////////////////////////////////////////////////////////////
// WindowGtk, private:

// static
gboolean WindowGtk::CallConfigureEvent(GtkWidget* widget,
                                       GdkEventConfigure* event,
                                       WindowGtk* window_gtk) {
  return window_gtk->OnConfigureEvent(widget, event);
}

// static
gboolean WindowGtk::CallWindowStateEvent(GtkWidget* widget,
                                         GdkEventWindowState* event,
                                         WindowGtk* window_gtk) {
  return window_gtk->OnWindowStateEvent(widget, event);
}

void WindowGtk::SaveWindowPosition() {
  // The delegate may have gone away on us.
  if (!window_delegate_)
    return;

  bool maximized = window_state_ & GDK_WINDOW_STATE_MAXIMIZED;
  gfx::Rect bounds;
  WidgetGtk::GetBounds(&bounds, true);
  window_delegate_->SaveWindowPlacement(bounds, maximized);
}

void WindowGtk::SetInitialBounds(const gfx::Rect& create_bounds) {
  gfx::Rect saved_bounds(create_bounds.ToGdkRectangle());
  if (window_delegate_->GetSavedWindowBounds(&saved_bounds)) {
    WidgetGtk::SetBounds(saved_bounds);
  } else {
    if (create_bounds.IsEmpty()) {
      SizeWindowToDefault();
    } else {
      SetBounds(create_bounds, NULL);
    }
  }
}

void WindowGtk::SizeWindowToDefault() {
  gfx::Size size = non_client_view_->GetPreferredSize();
  gfx::Rect bounds(size.width(), size.height());
  SetBounds(bounds, NULL);
}

}  // namespace views
