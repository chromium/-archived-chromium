// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_WEBWIDGET_H__
#define WEBKIT_GLUE_WEBWIDGET_H__

#include "skia/ext/platform_canvas.h"
#include "webkit/glue/webtextdirection.h"

namespace gfx {
class Rect;
class Size;
}

class WebInputEvent;
class WebWidgetDelegate;

class WebWidget {
 public:
  WebWidget() {}

  // This method creates a WebWidget that is initially invisible and positioned
  // according to the given bounds relative to the specified parent window.
  // The caller is responsible for showing the WebWidget's view window (see
  // GetViewWindow) once it is ready to have the WebWidget appear on the screen.
  static WebWidget* Create(WebWidgetDelegate* delegate);

  // This method closes and deletes the WebWidget.
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
  virtual void Paint(skia::PlatformCanvas* canvas, const gfx::Rect& rect) = 0;

  // Called to inform the WebWidget of an input event.
  // Returns true if the event has been processed, false otherwise.
  virtual bool HandleInputEvent(const WebInputEvent* input_event) = 0;

  // Called to inform the WebWidget that mouse capture was lost.
  virtual void MouseCaptureLost() = 0;

  // Called to inform the WebWidget that it has gained or lost keyboard focus.
  virtual void SetFocus(bool enable) = 0;

  // Called to inform the webwidget of a composition event from IMM
  // (Input Method Manager).
  virtual bool ImeSetComposition(int string_type, int cursor_position,
                                 int target_start, int target_end,
                                 const std::wstring& ime_string) = 0;

  // Retrieve the status of this widget required by IME APIs.
  virtual bool ImeUpdateStatus(bool* enable_ime, gfx::Rect* caret_rect) = 0;

  // Changes the text direction of the selected input node.
  virtual void SetTextDirection(WebTextDirection direction) = 0;

 protected:
  virtual ~WebWidget() {}

 private:
  DISALLOW_EVIL_CONSTRUCTORS(WebWidget);
};

#endif  // #ifndef WEBKIT_GLUE_WEBWIDGET_H__
