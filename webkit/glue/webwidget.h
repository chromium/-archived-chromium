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

#ifndef WEBKIT_GLUE_WEBWIDGET_H__
#define WEBKIT_GLUE_WEBWIDGET_H__

#include "base/ref_counted.h"

namespace gfx {
class PlatformCanvas;
class Rect;
class Size;
}

class WebInputEvent;
class WebWidgetDelegate;

class WebWidget : public base::RefCounted<WebWidget> {
 public:
  WebWidget() {}
  virtual ~WebWidget() {}

  // This method creates a WebWidget that is initially invisible and positioned
  // according to the given bounds relative to the specified parent window.
  // The caller is responsible for showing the WebWidget's view window (see
  // GetViewWindow) once it is ready to have the WebWidget appear on the screen.
  static WebWidget* Create(WebWidgetDelegate* delegate);

  // This method closes the WebWidget.
  virtual void Close() = 0;

  // Called to resize the WebWidget.
  virtual void Resize(const gfx::Size& new_size) = 0;

  // Returns the current size of the WebWidget.
  virtual gfx::Size GetSize() = 0;

  // Called to layout the WebWidget.  This MUST be called before Paint, and it
  // may result in calls to WebWidgetDelegate::DidInvalidateRect.
  virtual void Layout() = 0;

  // Called to paint the specified region of the WebWidget onto the given canvas.
  // You MUST call Layout before calling this method.  It is okay to call Paint
  // multiple times once Layout has been called, assuming no other changes are
  // made to the WebWidget (e.g., once events are processed, it should be assumed
  // that another call to Layout is warranted before painting again).
  virtual void Paint(gfx::PlatformCanvas* canvas, const gfx::Rect& rect) = 0;

  // Called to inform the WebWidget of an input event.
  // Returns true if the event has been processed, false otherwise.
  virtual bool HandleInputEvent(const WebInputEvent* input_event) = 0;

  // Called to inform the WebWidget that mouse capture was lost.
  virtual void MouseCaptureLost() = 0;

  // Called to inform the WebWidget that it has gained or lost keyboard focus.
  virtual void SetFocus(bool enable) = 0;

  // Called to inform the webwidget of a composition event from IMM
  // (Input Method Manager).
  virtual void ImeSetComposition(int string_type, int cursor_position,
                                 int target_start, int target_end,
                                 int string_length,
                                 const wchar_t *string_data) = 0;

  // Retrieve the status of this widget required by IME APIs.
  virtual bool ImeUpdateStatus(bool* enable_ime, const void** node,
                               int* x, int* y) = 0;

 private:
  DISALLOW_EVIL_CONSTRUCTORS(WebWidget);
};

#endif  // #ifndef WEBKIT_GLUE_WEBWIDGET_H__
