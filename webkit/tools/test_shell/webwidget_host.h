// Copyright 2008, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#ifndef WEBKIT_TOOLS_TEST_SHELL_WEBWIDGET_HOST_H__
#define WEBKIT_TOOLS_TEST_SHELL_WEBWIDGET_HOST_H__

#include <windows.h>

#include "base/gfx/rect.h"
#include "base/scoped_ptr.h"

class WebWidget;
class WebWidgetDelegate;

namespace gfx {
class PlatformCanvas;
class Size;
}

// This class is a simple HWND-based host for a WebWidget
class WebWidgetHost {
 public:
  // The new instance is deleted once the associated HWND is destroyed.  The
  // newly created window should be resized after it is created, using the
  // MoveWindow (or equivalent) function.
  static WebWidgetHost* Create(HWND parent_window, WebWidgetDelegate* delegate);

  static WebWidgetHost* FromWindow(HWND hwnd);

  HWND window_handle() const { return hwnd_; }
  WebWidget* webwidget() const { return webwidget_; }

  void DidInvalidateRect(const gfx::Rect& rect);
  void DidScrollRect(int dx, int dy, const gfx::Rect& clip_rect);
  void SetCursor(HCURSOR cursor);

  void DiscardBackingStore();

 protected:
  WebWidgetHost();
  ~WebWidgetHost();

  // Per-class wndproc.  Returns true if the event should be swallowed.
  virtual bool WndProc(UINT message, WPARAM wparam, LPARAM lparam);

  void Paint();
  void Resize(LPARAM lparam);
  void MouseEvent(UINT message, WPARAM wparam, LPARAM lparam);
  void WheelEvent(WPARAM wparam, LPARAM lparam);
  void KeyEvent(UINT message, WPARAM wparam, LPARAM lparam);
  void CaptureLostEvent();
  void SetFocus(bool enable);

  void TrackMouseLeave(bool enable);
  void ResetScrollRect();
  void PaintRect(const gfx::Rect& rect);

  void set_painting(bool value) {
#ifndef NDEBUG
    painting_ = value;
#endif
  }

  static LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);

  HWND hwnd_;
  WebWidget* webwidget_;
  scoped_ptr<gfx::PlatformCanvas> canvas_;

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

#endif  // WEBKIT_TOOLS_TEST_SHELL_WEBWIDGET_HOST_H__
