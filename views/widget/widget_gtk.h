// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VIEWS_WIDGET_WIDGET_GTK_H_
#define VIEWS_WIDGET_WIDGET_GTK_H_

#include <gtk/gtk.h>

#include "base/message_loop.h"
#include "views/widget/widget.h"

namespace gfx {
class Rect;
}

namespace views {

class DefaultThemeProvider;
class TooltipManagerGtk;
class View;
class WindowGtk;

// Widget implementation for GTK.
class WidgetGtk : public Widget, public MessageLoopForUI::Observer {
 public:
  // Type of widget.
  enum Type {
    // Used for popup type windows (bubbles, menus ...).
    TYPE_POPUP,
    // A top level window.
    TYPE_WINDOW,

    // A child widget.
    TYPE_CHILD
  };

  explicit WidgetGtk(Type type);
  virtual ~WidgetGtk();

  // Initializes this widget.
  void Init(GtkWidget* parent,
            const gfx::Rect& bounds,
            bool has_own_focus_manager);

  // Makes the background of the window totally transparent. This must be
  // invoked before Init. This does a couple of checks and returns true if
  // the window can be made transparent. The actual work of making the window
  // transparent is done by ConfigureWidgetForTransparentBackground.
  bool MakeTransparent();
  bool is_transparent() const { return transparent_; }

  // Sets whether or not we are deleted when the widget is destroyed. The
  // default is true.
  void set_delete_on_destroy(bool delete_on_destroy) {
    delete_on_destroy_ = delete_on_destroy;
  }

  // Adds and removes the specified widget as a child of this widget's contents.
  // These methods make sure to add the widget to the window's contents
  // container if this widget is a window.
  void AddChild(GtkWidget* child);
  void RemoveChild(GtkWidget* child);

  // A safe way to reparent a child widget to this widget. Calls
  // gtk_widget_reparent which handles refcounting to avoid destroying the
  // widget when removing it from its old parent.
  void ReparentChild(GtkWidget* child);

  // Positions a child GtkWidget at the specified location and bounds.
  void PositionChild(GtkWidget* child, int x, int y, int w, int h);

  // Parent GtkWidget all children are added to. When this WidgetGtk corresponds
  // to a top level window, this is the GtkFixed within the GtkWindow, not the
  // GtkWindow itself. For child widgets, this is the same GtkFixed as
  // |widget_|.
  GtkWidget* window_contents() const { return window_contents_; }

  virtual void SetContentsView(View* view);

  // Overridden from Widget:
  virtual void GetBounds(gfx::Rect* out, bool including_frame) const;
  virtual void SetBounds(const gfx::Rect& bounds);
  virtual void SetShape(const gfx::Path& shape);
  virtual void Close();
  virtual void CloseNow();
  virtual void Show();
  virtual void Hide();
  virtual gfx::NativeView GetNativeView() const;
  virtual void PaintNow(const gfx::Rect& update_rect);
  virtual void SetOpacity(unsigned char opacity);
  virtual RootView* GetRootView();
  virtual Widget* GetRootWidget() const;
  virtual bool IsVisible() const;
  virtual bool IsActive() const;
  virtual TooltipManager* GetTooltipManager();
  virtual bool GetAccelerator(int cmd_id, Accelerator* accelerator);
  virtual Window* GetWindow();
  virtual const Window* GetWindow() const;
  virtual ThemeProvider* GetThemeProvider() const;

  // MessageLoopForUI::Observer.
  virtual void WillProcessEvent(GdkEvent* event) {}
  virtual void DidProcessEvent(GdkEvent* event);

 protected:
  virtual void OnSizeAllocate(GtkWidget* widget, GtkAllocation* allocation);
  virtual void OnPaint(GtkWidget* widget, GdkEventExpose* event);
  virtual gboolean OnEnterNotify(GtkWidget* widget, GdkEventCrossing* event);
  virtual gboolean OnLeaveNotify(GtkWidget* widget, GdkEventCrossing* event);
  virtual gboolean OnMotionNotify(GtkWidget* widget, GdkEventMotion* event);
  virtual gboolean OnButtonPress(GtkWidget* widget, GdkEventButton* event);
  virtual gboolean OnButtonRelease(GtkWidget* widget, GdkEventButton* event);
  virtual gboolean OnFocusIn(GtkWidget* widget, GdkEventFocus* event) {
    return false;
  }
  virtual gboolean OnFocusOut(GtkWidget* widget, GdkEventFocus* event) {
    return false;
  }
  virtual gboolean OnKeyPress(GtkWidget* widget, GdkEventKey* event);
  virtual gboolean OnKeyRelease(GtkWidget* widget, GdkEventKey* event);
  virtual gboolean OnScroll(GtkWidget* widget, GdkEventScroll* event) {
    return false;
  }
  virtual gboolean OnVisibilityNotify(GtkWidget* widget,
                                      GdkEventVisibility* event) {
    return false;
  }
  virtual gboolean OnGrabBrokeEvent(GtkWidget* widget, GdkEvent* event);
  virtual void OnGrabNotify(GtkWidget* widget, gboolean was_grabbed);
  virtual void OnDestroy(GtkWidget* widget);

  // Returns whether capture should be released on mouse release. The default
  // is true.
  virtual bool ReleaseCaptureOnMouseReleased() { return true; }

  // Sets and retrieves the WidgetGtk in the userdata section of the widget.
  static WindowGtk* GetWindowForNative(GtkWidget* widget);
  static void SetWindowForNative(GtkWidget* widget, WindowGtk* window);

  // Are we a subclass of WindowGtk?
  bool is_window_;

 private:
  virtual RootView* CreateRootView();

  void OnWindowPaint(GtkWidget* widget, GdkEventExpose* event);

  // Process a mouse click
  bool ProcessMousePressed(GdkEventButton* event);
  void ProcessMouseReleased(GdkEventButton* event);

  // Sets and retrieves the WidgetGtk in the userdata section of the widget.
  static WidgetGtk* GetViewForNative(GtkWidget* widget);
  static void SetViewForNative(GtkWidget* widget, WidgetGtk* view);

  static RootView* GetRootViewForWidget(GtkWidget* widget);
  static void SetRootViewForWidget(GtkWidget* widget, RootView* root_view);

  // A set of static signal handlers that bridge
  // TODO(beng): alphabetize!
  static void CallSizeAllocate(GtkWidget* widget, GtkAllocation* allocation);
  static gboolean CallPaint(GtkWidget* widget, GdkEventExpose* event);
  static gboolean CallWindowPaint(GtkWidget* widget,
                                  GdkEventExpose* event,
                                  WidgetGtk* widget_gtk);
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
  static gboolean CallGrabBrokeEvent(GtkWidget* widget, GdkEvent* event);
  static void CallGrabNotify(GtkWidget* widget, gboolean was_grabbed);
  static void CallDestroy(GtkObject* object);

  // Returns the first ancestor of |widget| that is a window.
  static Window* GetWindowImpl(GtkWidget* widget);

  // Creates the GtkWidget.
  void CreateGtkWidget(GtkWidget* parent, const gfx::Rect& bounds);

  // Attaches the widget contents to the window's widget.
  void AttachGtkWidgetToWindow();

  // Invoked from create widget to enable the various bits needed for a
  // transparent background. This is only invoked if MakeTransparent has been
  // invoked.
  void ConfigureWidgetForTransparentBackground();

  void HandleGrabBroke();

  const Type type_;

  // Our native views. If we're a window/popup, then widget_ is the window and
  // window_contents_ is a GtkFixed. If we're not a window/popup, then widget_
  // and window_contents_ point to the same GtkFixed.
  GtkWidget* widget_;
  GtkWidget* window_contents_;

  // Child GtkWidgets created with no parent need to be parented to a valid top
  // level window otherwise Gtk throws a fit. |null_parent_| is an invisible
  // popup that such GtkWidgets are parented to.
  static GtkWidget* null_parent_;

  // The root of the View hierarchy attached to this window.
  scoped_ptr<RootView> root_view_;

  scoped_ptr<TooltipManagerGtk> tooltip_manager_;

  // If true, the mouse is currently down.
  bool is_mouse_down_;

  // Have we done a mouse grab?
  bool has_capture_;

  // The following are used to detect duplicate mouse move events and not
  // deliver them. Displaying a window may result in the system generating
  // duplicate move events even though the mouse hasn't moved.

  // If true, the last event was a mouse move event.
  bool last_mouse_event_was_move_;

  // Coordinates of the last mouse move event, in screen coordinates.
  int last_mouse_move_x_;
  int last_mouse_move_y_;

  // The following factory is used to delay destruction.
  ScopedRunnableMethodFactory<WidgetGtk> close_widget_factory_;

  // See description above setter.
  bool delete_on_destroy_;

  // See description above make_transparent for details.
  bool transparent_;

  scoped_ptr<DefaultThemeProvider> default_theme_provider_;

  DISALLOW_COPY_AND_ASSIGN(WidgetGtk);
};

}  // namespace views

#endif  // VIEWS_WIDGET_WIDGET_GTK_H_
