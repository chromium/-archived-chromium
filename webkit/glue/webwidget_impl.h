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

#ifndef WEBKIT_GLUE_WEBWIDGET_IMPL_H__
#define WEBKIT_GLUE_WEBWIDGET_IMPL_H__

#include "base/basictypes.h"
#include "base/gfx/point.h"
#include "base/gfx/size.h"
#include "webkit/glue/webwidget.h"

#pragma warning(push, 0)
#include "WidgetClientWin.h"
#pragma warning(pop)

namespace WebCore {
  class Frame;
  class FramelessScrollView;
  class KeyboardEvent;
  class Page;
  class PlatformKeyboardEvent;
  class Range;
  class Widget;
}

class WebKeyboardEvent;
class WebMouseEvent;
class WebMouseWheelEvent;
class WebWidgetDelegate;

class WebWidgetImpl : public WebWidget, public WebCore::WidgetClientWin {
 public:
  // WebWidget
  virtual void Close();
  virtual void Resize(const gfx::Size& new_size);
  virtual gfx::Size GetSize() { return size(); }
  virtual void Layout();
  virtual void Paint(gfx::PlatformCanvasWin* canvas, const gfx::Rect& rect);
  virtual bool HandleInputEvent(const WebInputEvent* input_event);
  virtual void MouseCaptureLost();
  virtual void SetFocus(bool enable);
  virtual void ImeSetComposition(int string_type, int cursor_position,
                                 int target_start, int target_end,
                                 int string_length,
                                 const wchar_t *string_data);
  virtual bool ImeUpdateStatus(bool* enable_ime, const void** id,
                               int* x, int* y);

  // WebWidgetImpl
  void Init(WebCore::Widget* widget, const gfx::Rect& bounds);

  const gfx::Size& size() const { return size_; }

  WebWidgetDelegate* delegate() {
    return delegate_;
  }

  void MouseMove(const WebMouseEvent& mouse_event);
  void MouseLeave(const WebMouseEvent& mouse_event);
  void MouseDown(const WebMouseEvent& mouse_event);
  void MouseUp(const WebMouseEvent& mouse_event);
  void MouseDoubleClick(const WebMouseEvent& mouse_event);
  void MouseWheel(const WebMouseWheelEvent& wheel_event);
  bool KeyEvent(const WebKeyboardEvent& key_event);

 protected:
  friend class WebWidget;  // So WebWidget::Create can call our constructor

  WebWidgetImpl(WebWidgetDelegate* delegate);
  ~WebWidgetImpl();

  // WebCore::WidgetClientWin
  virtual HWND containingWindow();
  virtual void invalidateRect(const WebCore::IntRect& damaged_rect);
  virtual void scrollRect(int dx, int dy, const WebCore::IntRect& clip_rect);
  virtual void popupOpened(WebCore::Widget* widget,
                           const WebCore::IntRect& bounds);
  virtual void popupClosed(WebCore::Widget* widget);
  virtual void setCursor(const WebCore::Cursor& cursor);
  virtual void setFocus();
  virtual const SkBitmap* getPreloadedResourceBitmap(int resource_id);
  virtual void onScrollPositionChanged(WebCore::Widget* widget);
  virtual const WTF::Vector<RefPtr<WebCore::Range> >* getTickmarks(
      WebCore::Frame* frame);
  virtual size_t getActiveTickmarkIndex(WebCore::Frame* frame);

  WebWidgetDelegate* delegate_;
  gfx::Size size_;

  gfx::Point last_mouse_position_;

  // This is a non-owning ref.  The popup will notify us via popupClosed()
  // before it is destroyed.
  WebCore::FramelessScrollView* widget_;

 private:
  DISALLOW_EVIL_CONSTRUCTORS(WebWidgetImpl);
};

#endif  // WEBKIT_GLUE_WEBWIDGET_IMPL_H__
