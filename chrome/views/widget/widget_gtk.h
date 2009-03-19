// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_VIEWS_WIDGET_WIDGET_GTK_H_
#define CHROME_VIEWS_WIDGET_WIDGET_GTK_H_

#include <gtk/gtk.h>

#include "base/message_loop.h"
#include "chrome/views/widget/widget.h"

namespace gfx {
class Rect;
}

namespace views {

class View;

class WidgetGtk : public Widget {
 public:
  static WidgetGtk* Construct() {
    // This isn't used, but exists to force WidgetGtk to be instantiable.
    return new WidgetGtk;
  }

  WidgetGtk();
  virtual ~WidgetGtk();

  // Initializes this widget and returns the gtk drawing area for the caller to
  // add to its hierarchy. (We can't pass in the parent to this method because
  // there are no standard adding semantics in gtk...)
  void Init(const gfx::Rect& bounds, bool has_own_focus_manager);

  virtual void SetContentsView(View* view);

  // Overridden from Widget:
  virtual void GetBounds(gfx::Rect* out, bool including_frame) const;
  virtual void MoveToFront(bool should_activate);
  virtual gfx::NativeView GetNativeView() const;
  virtual void PaintNow(const gfx::Rect& update_rect);
  virtual RootView* GetRootView();
  virtual bool IsVisible();
  virtual bool IsActive();
  virtual TooltipManager* GetTooltipManager();
  virtual bool GetAccelerator(int cmd_id, Accelerator* accelerator);

 protected:
  virtual void OnSizeAllocate(GtkWidget* widget, GtkAllocation* allocation) {}
  virtual gboolean OnPaint(GtkWidget* widget, GdkEventExpose* event);
  virtual gboolean OnEnterNotify(GtkWidget* widget, GdkEventCrossing* event) {
    return false;
  }
  virtual gboolean OnLeaveNotify(GtkWidget* widget, GdkEventCrossing* event) {
    return false;
  }
  virtual gboolean OnMotionNotify(GtkWidget* widget, GdkEventMotion* event) {
    return false;
  }
  virtual gboolean OnButtonPress(GtkWidget* widget, GdkEventButton* event) {
    return false;
  }
  virtual gboolean OnButtonRelease(GtkWidget* widget, GdkEventButton* event) {
    return false;
  }
  virtual gboolean OnFocusIn(GtkWidget* widget, GdkEventFocus* event) {
    return false;
  }
  virtual gboolean OnFocusOut(GtkWidget* widget, GdkEventFocus* event) {
    return false;
  }
  virtual gboolean OnKeyPress(GtkWidget* widget, GdkEventKey* event) {
    return false;
  }
  virtual gboolean OnKeyRelease(GtkWidget* widget, GdkEventKey* event) {
    return false;
  }
  virtual gboolean OnScroll(GtkWidget* widget, GdkEventScroll* event) {
    return false;
  }
  virtual gboolean OnVisibilityNotify(GtkWidget* widget,
                                      GdkEventVisibility* event) {
    return false;
  }

 private:
  virtual RootView* CreateRootView();

  // Sets and retrieves the WidgetGtk in the userdata section of the widget.
  static WidgetGtk* GetViewForNative(GtkWidget* widget);
  static void SetViewForNative(GtkWidget* widget, WidgetGtk* view);

  static RootView* GetRootViewForWidget(GtkWidget* widget);
  static void SetRootViewForWidget(GtkWidget* widget, RootView* root_view);

  // A set of static signal handlers that bridge
  static void CallSizeAllocate(GtkWidget* widget, GtkAllocation* allocation);
  static gboolean CallPaint(GtkWidget* widget, GdkEventExpose* event);
  static gboolean CallEnterNotify(GtkWidget* widget, GdkEventCrossing* event);
  static gboolean CallLeaveNotify(GtkWidget* widget, GdkEventCrossing* event);
  static gboolean CallMotionNotify(GtkWidget* widget, GdkEventMotion* event);
  static gboolean CallButtonPress(GtkWidget* widget, GdkEventButton* event);
  static gboolean CallButtonRelease(GtkWidget* widget, GdkEventButton* event);
  static gboolean CallFocusIn(GtkWidget* widget, GdkEventFocus* event);
  static gboolean CallFocusOut(GtkWidget* widget, GdkEventFocus* event);
  static gboolean CallKeyPress(GtkWidget* widget, GdkEventKey* event);
  static gboolean CallKeyRelease(GtkWidget* widget, GdkEventKey* event);
  static gboolean CallScroll(GtkWidget* widget, GdkEventScroll* event);
  static gboolean CallVisibilityNotify(GtkWidget* widget,
                                       GdkEventVisibility* event);

  // Our native view.
  GtkWidget* widget_;

  // The root of the View hierarchy attached to this window.
  scoped_ptr<RootView> root_view_;
};

}  // namespace views

#endif  // CHROME_VIEWS_WIDGET_WIDGET_GTK_H_
