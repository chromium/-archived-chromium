// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_TOOLS_TEST_SHELL_WEBWIDGET_HOST_H_
#define WEBKIT_TOOLS_TEST_SHELL_WEBWIDGET_HOST_H_

#include "base/basictypes.h"
#include "base/gfx/native_widget_types.h"
#include "base/gfx/rect.h"
#include "base/scoped_ptr.h"
#include "skia/ext/platform_canvas.h"

class WebWidget;
class WebWidgetDelegate;

namespace gfx {
class Size;
}

namespace WebKit {
struct WebScreenInfo;
}

#if defined(OS_MACOSX)
#ifdef __OBJC__
@class NSEvent;
#else
class NSEvent;
#endif
#endif

// This class is a simple NativeView-based host for a WebWidget
class WebWidgetHost {
 public:
  // The new instance is deleted once the associated NativeView is destroyed.
  // The newly created window should be resized after it is created, using the
  // MoveWindow (or equivalent) function.
  static WebWidgetHost* Create(gfx::NativeView parent_view,
                               WebWidgetDelegate* delegate);

#if defined(OS_MACOSX)
  static void HandleEvent(gfx::NativeView view, NSEvent* event);
#endif

  gfx::NativeView view_handle() const { return view_; }
  WebWidget* webwidget() const { return webwidget_; }

  void DidInvalidateRect(const gfx::Rect& rect);
  void DidScrollRect(int dx, int dy, const gfx::Rect& clip_rect);
#if defined(OS_WIN)
  void SetCursor(HCURSOR cursor);
#endif

  void DiscardBackingStore();
  // Allow clients to update the paint rect. For example, if we get a gdk
  // expose or WM_PAINT event, we need to update the paint rect.
  void UpdatePaintRect(const gfx::Rect& rect);
  void Paint();

  skia::PlatformCanvas* canvas() const { return canvas_.get(); }

  WebKit::WebScreenInfo GetScreenInfo();

 protected:
  WebWidgetHost();
  ~WebWidgetHost();

#if defined(OS_WIN)
  // Per-class wndproc.  Returns true if the event should be swallowed.
  virtual bool WndProc(UINT message, WPARAM wparam, LPARAM lparam);

  void Resize(LPARAM lparam);
  void MouseEvent(UINT message, WPARAM wparam, LPARAM lparam);
  void WheelEvent(WPARAM wparam, LPARAM lparam);
  void KeyEvent(UINT message, WPARAM wparam, LPARAM lparam);
  void CaptureLostEvent();
  void SetFocus(bool enable);

  static LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
#elif defined(OS_MACOSX)
  // These need to be called from a non-subclass, so they need to be public.
 public:
  void Resize(const gfx::Rect& rect);
  void MouseEvent(NSEvent *);
  void WheelEvent(NSEvent *);
  void KeyEvent(NSEvent *);
  void SetFocus(bool enable);
 protected:
#elif defined(OS_LINUX)
 public:
  // ---------------------------------------------------------------------------
  // This is needed on Linux because the GtkWidget creation is the same between
  // both web view hosts and web widget hosts. The Windows code manages this by
  // reusing the WndProc function (static, above). However, GTK doesn't use a
  // single big callback function like that so we have a static function that
  // sets up a GtkWidget correctly.
  //   parent: a GtkBox to pack the new widget at the end of
  //   host: a pointer to a WebWidgetHost (or subclass thereof)
  // ---------------------------------------------------------------------------
  static gfx::NativeView CreateWidget(gfx::NativeView parent_view,
                                      WebWidgetHost* host);
  void WindowDestroyed();
  void Resize(const gfx::Size& size);
#endif

  void TrackMouseLeave(bool enable);
  void ResetScrollRect();
  void PaintRect(const gfx::Rect& rect);

  void set_painting(bool value) {
#ifndef NDEBUG
    painting_ = value;
#endif
  }

  gfx::NativeView view_;
  WebWidget* webwidget_;
  scoped_ptr<skia::PlatformCanvas> canvas_;

  // specifies the portion of the webwidget that needs painting
  gfx::Rect paint_rect_;

  // specifies the portion of the webwidget that needs scrolling
  gfx::Rect scroll_rect_;
  int scroll_dx_;
  int scroll_dy_;

  bool track_mouse_leave_;

#ifndef NDEBUG
  bool painting_;
#endif
};

#endif  // WEBKIT_TOOLS_TEST_SHELL_WEBWIDGET_HOST_H_
