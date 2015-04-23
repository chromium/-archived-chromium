// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VIEWS_WINDOW_WINDOW_GTK_H_
#define VIEWS_WINDOW_WINDOW_GTK_H_

#include "base/basictypes.h"
#include "views/widget/widget_gtk.h"
#include "views/window/window.h"

namespace gfx {
class Point;
class Size;
};

namespace views {

class Client;
class WindowDelegate;

// Window implementation for GTK.
class WindowGtk : public WidgetGtk, public Window {
 public:
  virtual ~WindowGtk();

  // Overridden from Window:
  virtual gfx::Rect GetBounds() const;
  virtual gfx::Rect GetNormalBounds() const;
  virtual void SetBounds(const gfx::Rect& bounds,
                         gfx::NativeWindow other_window);
  virtual void Show();
  virtual void HideWindow();
  virtual void PushForceHidden();
  virtual void PopForceHidden();
  virtual void Activate();
  virtual void Close();
  virtual void Maximize();
  virtual void Minimize();
  virtual void Restore();
  virtual bool IsActive() const;
  virtual bool IsVisible() const;
  virtual bool IsMaximized() const;
  virtual bool IsMinimized() const;
  virtual void SetFullscreen(bool fullscreen);
  virtual bool IsFullscreen() const;
  virtual void EnableClose(bool enable);
  virtual void DisableInactiveRendering();
  virtual void UpdateWindowTitle();
  virtual void UpdateWindowIcon();
  virtual void SetIsAlwaysOnTop(bool always_on_top);
  virtual NonClientFrameView* CreateFrameViewForWindow();
  virtual void UpdateFrameAfterFrameChange();
  virtual WindowDelegate* GetDelegate() const;
  virtual NonClientView* GetNonClientView() const;
  virtual ClientView* GetClientView() const;
  virtual gfx::NativeWindow GetNativeWindow() const;
  virtual bool ShouldUseNativeFrame() const;
  virtual void FrameTypeChanged();

  virtual Window* AsWindow() { return this; }
  virtual const Window* AsWindow() const { return this; }

  // Overridden from WidgetGtk:
  virtual gboolean OnButtonPress(GtkWidget* widget, GdkEventButton* event);
  virtual gboolean OnConfigureEvent(GtkWidget* widget,
                                    GdkEventConfigure* event);
  virtual gboolean OnMotionNotify(GtkWidget* widget, GdkEventMotion* event);
  virtual void OnSizeAllocate(GtkWidget* widget, GtkAllocation* allocation);
  virtual gboolean OnWindowStateEvent(GtkWidget* widget,
                                      GdkEventWindowState* event);

 protected:
  // For  the constructor.
  friend class Window;

  // Constructs the WindowGtk. |window_delegate| cannot be NULL.
  explicit WindowGtk(WindowDelegate* window_delegate);

  // Initializes the window to the passed in bounds.
  void Init(const gfx::Rect& bounds);

 private:
  static gboolean CallConfigureEvent(GtkWidget* widget,
                                     GdkEventConfigure* event,
                                     WindowGtk* window_gtk);
  static gboolean CallWindowStateEvent(GtkWidget* widget,
                                       GdkEventWindowState* event,
                                       WindowGtk* window_gtk);

  // Asks the delegate if any to save the window's location and size.
  void SaveWindowPosition();

  void SetInitialBounds(const gfx::Rect& bounds);
  void SizeWindowToDefault();

  // Whether or not the window is modal. This comes from the delegate and is
  // cached at Init time to avoid calling back to the delegate from the
  // destructor.
  bool is_modal_;

  // Our window delegate.
  WindowDelegate* window_delegate_;

  // The View that provides the non-client area of the window (title bar,
  // window controls, sizing borders etc). To use an implementation other than
  // the default, this class must be subclassed and this value set to the
  // desired implementation before calling |Init|.
  NonClientView* non_client_view_;

  // State of the window, such as fullscreen, hidden...
  GdkWindowState window_state_;

  // Set to true if the window is in the process of closing.
  bool window_closed_;

  DISALLOW_COPY_AND_ASSIGN(WindowGtk);
};

}  // namespace views

#endif  // VIEWS_WINDOW_WINDOW_GTK_H_
