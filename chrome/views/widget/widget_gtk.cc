// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/views/widget/widget_gtk.h"

#include "chrome/views/fill_layout.h"
#include "chrome/views/widget/root_view.h"

namespace views {

WidgetGtk::WidgetGtk()
    : widget_(NULL),
      is_mouse_down_(false),
      last_mouse_event_was_move_(false) {
}

WidgetGtk::~WidgetGtk() {
  gtk_widget_unref(widget_);

  // MessageLoopForUI::current()->RemoveObserver(this);
}

void WidgetGtk::Init(const gfx::Rect& bounds,
                     bool has_own_focus_manager) {

  // Force creation of the RootView if it hasn't been created yet.
  GetRootView();

  // Make container here.
  widget_ = gtk_drawing_area_new();
  gtk_drawing_area_size(GTK_DRAWING_AREA(widget_), 100, 100);
  gtk_widget_show(widget_);

  // Make sure we receive our motion events.
  gtk_widget_set_events(widget_,
                        gtk_widget_get_events(widget_) |
                        GDK_ENTER_NOTIFY_MASK |
                        GDK_LEAVE_NOTIFY_MASK |
                        GDK_BUTTON_PRESS_MASK |
                        GDK_BUTTON_RELEASE_MASK |
                        GDK_POINTER_MOTION_MASK |
                        GDK_KEY_PRESS_MASK |
                        GDK_KEY_RELEASE_MASK);

  root_view_->OnWidgetCreated();

  // TODO(port): if(has_own_focus_manager) block

  SetViewForNative(widget_, this);
  SetRootViewForWidget(widget_, root_view_.get());

  // MessageLoopForUI::current()->AddObserver(this);

  g_signal_connect_after(G_OBJECT(widget_), "size_allocate",
                         G_CALLBACK(CallSizeAllocate), NULL);
  g_signal_connect(G_OBJECT(widget_), "expose_event",
                   G_CALLBACK(CallPaint), NULL);
  g_signal_connect(G_OBJECT(widget_), "enter_notify_event",
                   G_CALLBACK(CallEnterNotify), NULL);
  g_signal_connect(G_OBJECT(widget_), "leave_notify_event",
                   G_CALLBACK(CallLeaveNotify), NULL);
  g_signal_connect(G_OBJECT(widget_), "motion_notify_event",
                   G_CALLBACK(CallMotionNotify), NULL);
  g_signal_connect(G_OBJECT(widget_), "button_press_event",
                   G_CALLBACK(CallButtonPress), NULL);
  g_signal_connect(G_OBJECT(widget_), "button_release_event",
                   G_CALLBACK(CallButtonRelease), NULL);
  g_signal_connect(G_OBJECT(widget_), "focus_in_event",
                   G_CALLBACK(CallFocusIn), NULL);
  g_signal_connect(G_OBJECT(widget_), "focus_out_event",
                   G_CALLBACK(CallFocusOut), NULL);
  g_signal_connect(G_OBJECT(widget_), "key_press_event",
                   G_CALLBACK(CallKeyPress), NULL);
  g_signal_connect(G_OBJECT(widget_), "key_release_event",
                   G_CALLBACK(CallKeyRelease), NULL);
  g_signal_connect(G_OBJECT(widget_), "scroll_event",
                   G_CALLBACK(CallScroll), NULL);
  g_signal_connect(G_OBJECT(widget_), "visibility_notify_event",
                   G_CALLBACK(CallVisibilityNotify), NULL);

  // TODO(erg): Ignore these signals for now because they're such a drag.
  //
  // g_signal_connect(G_OBJECT(widget_), "drag_motion",
  //                  G_CALLBACK(drag_motion_event_cb), NULL);
  // g_signal_connect(G_OBJECT(widget_), "drag_leave",
  //                  G_CALLBACK(drag_leave_event_cb), NULL);
  // g_signal_connect(G_OBJECT(widget_), "drag_drop",
  //                  G_CALLBACK(drag_drop_event_cb), NULL);
  // g_signal_connect(G_OBJECT(widget_), "drag_data_received",
  //                  G_CALLBACK(drag_data_received_event_cb), NULL);
}

void WidgetGtk::SetContentsView(View* view) {
  DCHECK(view && widget_) << "Can't be called until after the HWND is created!";
  // The ContentsView must be set up _after_ the window is created so that its
  // Widget pointer is valid.
  root_view_->SetLayoutManager(new FillLayout);
  if (root_view_->GetChildViewCount() != 0)
    root_view_->RemoveAllChildViews(true);
  root_view_->AddChildView(view);

  // TODO(erg): Terrible hack to work around lack of real sizing mechanics for
  // now.
  root_view_->SetBounds(0, 0, 100, 100);
  root_view_->Layout();
  root_view_->SchedulePaint();
  NOTIMPLEMENTED();
}

void WidgetGtk::GetBounds(gfx::Rect* out, bool including_frame) const {
  if (including_frame) {
    NOTIMPLEMENTED();
    *out = gfx::Rect();
    return;
  }

  // TODO(erg): Not sure how to implement this. gtk_widget_size_request()
  // returns a widget's requested size--not it's actual size. The system of
  // containers and such do auto sizing tricks to make everything work within
  // the constraints and requested sizes...
  NOTIMPLEMENTED();
}

void WidgetGtk::MoveToFront(bool should_activate) {
  // TODO(erg): I'm not sure about how to do z-ordering on GTK widgets...
  NOTIMPLEMENTED();
}

gfx::NativeView WidgetGtk::GetNativeView() const {
  return widget_;
}

void WidgetGtk::PaintNow(const gfx::Rect& update_rect) {
  // TODO(erg): This is woefully incomplete and is a straw man implementation.
  gtk_widget_queue_draw_area(widget_, update_rect.x(), update_rect.y(),
                             update_rect.width(), update_rect.height());
}

RootView* WidgetGtk::GetRootView() {
  if (!root_view_.get()) {
    // First time the root view is being asked for, create it now.
    root_view_.reset(CreateRootView());
  }
  return root_view_.get();
}

bool WidgetGtk::IsVisible() {
  return GTK_WIDGET_VISIBLE(widget_);
}

bool WidgetGtk::IsActive() {
  NOTIMPLEMENTED();
  return false;
}

TooltipManager* WidgetGtk::GetTooltipManager() {
  NOTIMPLEMENTED();
  return NULL;
}

bool WidgetGtk::GetAccelerator(int cmd_id, Accelerator* accelerator) {
  NOTIMPLEMENTED();
  return false;
}

gboolean WidgetGtk::OnMotionNotify(GtkWidget* widget, GdkEventMotion* event) {
  gfx::Point screen_loc(event->x_root, event->y_root);
  if (last_mouse_event_was_move_ && last_mouse_move_x_ == screen_loc.x() &&
      last_mouse_move_y_ == screen_loc.y()) {
    // Don't generate a mouse event for the same location as the last.
    return false;
  }
  last_mouse_move_x_ = screen_loc.x();
  last_mouse_move_y_ = screen_loc.y();
  last_mouse_event_was_move_ = true;
  MouseEvent mouse_move(Event::ET_MOUSE_MOVED,
                        event->x,
                        event->y,
                        Event::GetFlagsFromGdkState(event->state));
  root_view_->OnMouseMoved(mouse_move);
  return true;
}

gboolean WidgetGtk::OnButtonPress(GtkWidget* widget, GdkEventButton* event) {
  return ProcessMousePressed(event);
}

gboolean WidgetGtk::OnButtonRelease(GtkWidget* widget, GdkEventButton* event) {
  ProcessMouseReleased(event);
  return true;
}

gboolean WidgetGtk::OnPaint(GtkWidget* widget, GdkEventExpose* event) {
  root_view_->OnPaint(event);
  return true;
}

gboolean WidgetGtk::OnEnterNotify(GtkWidget* widget, GdkEventCrossing* event) {
  // TODO(port): We may not actually need this message; it looks like
  // OnNotificationNotify() takes care of this case...
  return false;
}

gboolean WidgetGtk::OnLeaveNotify(GtkWidget* widget, GdkEventCrossing* event) {
  last_mouse_event_was_move_ = false;
  root_view_->ProcessOnMouseExited();
  return true;
}

gboolean WidgetGtk::OnKeyPress(GtkWidget* widget, GdkEventKey* event) {
  KeyEvent key_event(event);
  return root_view_->ProcessKeyEvent(key_event);
}

gboolean WidgetGtk::OnKeyRelease(GtkWidget* widget, GdkEventKey* event) {
  KeyEvent key_event(event);
  return root_view_->ProcessKeyEvent(key_event);
}

RootView* WidgetGtk::CreateRootView() {
  return new RootView(this);
}

bool WidgetGtk::ProcessMousePressed(GdkEventButton* event) {
  last_mouse_event_was_move_ = false;
  MouseEvent mouse_pressed(Event::ET_MOUSE_PRESSED,
                           event->x, event->y,
//                         (dbl_click ? MouseEvent::EF_IS_DOUBLE_CLICK : 0) |
                           Event::GetFlagsFromGdkState(event->state));
  if (root_view_->OnMousePressed(mouse_pressed)) {
    is_mouse_down_ = true;
    // TODO(port): Enable this once I figure out what capture is.
    // if (!has_capture_) {
    //   SetCapture();
    //   has_capture_ = true;
    //   current_action_ = FA_FORWARDING;
    // }
    return true;
  }

  return false;
}

void WidgetGtk::ProcessMouseReleased(GdkEventButton* event) {
  last_mouse_event_was_move_ = false;
  MouseEvent mouse_up(Event::ET_MOUSE_RELEASED,
                      event->x, event->y,
                      Event::GetFlagsFromGdkState(event->state));
  // Release the capture first, that way we don't get confused if
  // OnMouseReleased blocks.
  //
  // TODO(port): Enable this once I figure out what capture is.
  // if (has_capture_ && ReleaseCaptureOnMouseReleased()) {
  //   has_capture_ = false;
  //   current_action_ = FA_NONE;
  //   ReleaseCapture();
  // }
  is_mouse_down_ = false;
  root_view_->OnMouseReleased(mouse_up, false);
}

// static
WidgetGtk* WidgetGtk::GetViewForNative(GtkWidget* widget) {
  gpointer user_data = g_object_get_data(G_OBJECT(widget), "chrome-views");
  return static_cast<WidgetGtk*>(user_data);
}

// static
void WidgetGtk::SetViewForNative(GtkWidget* widget, WidgetGtk* view) {
  g_object_set_data(G_OBJECT(widget), "chrome-views", view);
}

// static
RootView* WidgetGtk::GetRootViewForWidget(GtkWidget* widget) {
  gpointer user_data = g_object_get_data(G_OBJECT(widget), "root-view");
  return static_cast<RootView*>(user_data);
}

// static
void WidgetGtk::SetRootViewForWidget(GtkWidget* widget, RootView* root_view) {
  g_object_set_data(G_OBJECT(widget), "root-view", root_view);
}

// static
void WidgetGtk::CallSizeAllocate(GtkWidget* widget, GtkAllocation* allocation) {
  WidgetGtk* widget_gtk = GetViewForNative(widget);
  if (!widget_gtk)
    return;

  widget_gtk->OnSizeAllocate(widget, allocation);
}

gboolean WidgetGtk::CallPaint(GtkWidget* widget, GdkEventExpose* event) {
  WidgetGtk* widget_gtk = GetViewForNative(widget);
  if (!widget_gtk)
    return false;

  return widget_gtk->OnPaint(widget, event);
}

gboolean WidgetGtk::CallEnterNotify(GtkWidget* widget, GdkEventCrossing* event) {
  WidgetGtk* widget_gtk = GetViewForNative(widget);
  if (!widget_gtk)
    return false;

  return widget_gtk->OnEnterNotify(widget, event);
}

gboolean WidgetGtk::CallLeaveNotify(GtkWidget* widget, GdkEventCrossing* event) {
  WidgetGtk* widget_gtk = GetViewForNative(widget);
  if (!widget_gtk)
    return false;

  return widget_gtk->OnLeaveNotify(widget, event);
}

gboolean WidgetGtk::CallMotionNotify(GtkWidget* widget, GdkEventMotion* event) {
  WidgetGtk* widget_gtk = GetViewForNative(widget);
  if (!widget_gtk)
    return false;

  return widget_gtk->OnMotionNotify(widget, event);
}

gboolean WidgetGtk::CallButtonPress(GtkWidget* widget, GdkEventButton* event) {
  WidgetGtk* widget_gtk = GetViewForNative(widget);
  if (!widget_gtk)
    return false;

  return widget_gtk->OnButtonPress(widget, event);
}

gboolean WidgetGtk::CallButtonRelease(GtkWidget* widget, GdkEventButton* event) {
  WidgetGtk* widget_gtk = GetViewForNative(widget);
  if (!widget_gtk)
    return false;

  return widget_gtk->OnButtonRelease(widget, event);
}

gboolean WidgetGtk::CallFocusIn(GtkWidget* widget, GdkEventFocus* event) {
  WidgetGtk* widget_gtk = GetViewForNative(widget);
  if (!widget_gtk)
    return false;

  return widget_gtk->OnFocusIn(widget, event);
}

gboolean WidgetGtk::CallFocusOut(GtkWidget* widget, GdkEventFocus* event) {
  WidgetGtk* widget_gtk = GetViewForNative(widget);
  if (!widget_gtk)
    return false;

  return widget_gtk->OnFocusOut(widget, event);
}

gboolean WidgetGtk::CallKeyPress(GtkWidget* widget, GdkEventKey* event) {
  WidgetGtk* widget_gtk = GetViewForNative(widget);
  if (!widget_gtk)
    return false;

  return widget_gtk->OnKeyPress(widget, event);
}

gboolean WidgetGtk::CallKeyRelease(GtkWidget* widget, GdkEventKey* event) {
  WidgetGtk* widget_gtk = GetViewForNative(widget);
  if (!widget_gtk)
    return false;

  return widget_gtk->OnKeyRelease(widget, event);
}

gboolean WidgetGtk::CallScroll(GtkWidget* widget, GdkEventScroll* event) {
  WidgetGtk* widget_gtk = GetViewForNative(widget);
  if (!widget_gtk)
    return false;

  return widget_gtk->OnScroll(widget, event);
}

gboolean WidgetGtk::CallVisibilityNotify(GtkWidget* widget,
                                         GdkEventVisibility* event) {
  WidgetGtk* widget_gtk = GetViewForNative(widget);
  if (!widget_gtk)
    return false;

  return widget_gtk->OnVisibilityNotify(widget, event);
}

}
