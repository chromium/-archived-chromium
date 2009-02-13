// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_WEBWIDGET_IMPL_H__
#define WEBKIT_GLUE_WEBWIDGET_IMPL_H__

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/gfx/native_widget_types.h"
#include "base/gfx/point.h"
#include "base/gfx/size.h"
#include "webkit/glue/webwidget.h"

#include "FramelessScrollViewClient.h"

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

class WebWidgetImpl : public WebWidget,
                      public WebCore::FramelessScrollViewClient,
                      public base::RefCounted<WebWidgetImpl> {
 public:
  // WebWidget
  virtual void Close();
  virtual void Resize(const gfx::Size& new_size);
  virtual gfx::Size GetSize() { return size(); }
  virtual void Layout();
  virtual void Paint(skia::PlatformCanvas* canvas, const gfx::Rect& rect);
  virtual bool HandleInputEvent(const WebInputEvent* input_event);
  virtual void MouseCaptureLost();
  virtual void SetFocus(bool enable);
  virtual bool ImeSetComposition(int string_type,
                                 int cursor_position,
                                 int target_start,
                                 int target_end,
                                 const std::wstring& ime_string);
  virtual bool ImeUpdateStatus(bool* enable_ime,
                               const void** node,
                               gfx::Rect* caret_rect);

  // WebWidgetImpl
  void Init(WebCore::FramelessScrollView* widget, const gfx::Rect& bounds);

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
  friend class base::RefCounted<WebWidgetImpl>;

  WebWidgetImpl(WebWidgetDelegate* delegate);
  ~WebWidgetImpl();

  // WebCore::HostWindow methods:
  virtual void repaint(const WebCore::IntRect&,
                       bool content_changed,
                       bool immediate = false,
                       bool repaint_content_only = false);
  virtual void scroll(const WebCore::IntSize& scroll_delta,
                      const WebCore::IntRect& scroll_rect,
                      const WebCore::IntRect& clip_rect);
  virtual WebCore::IntPoint screenToWindow(const WebCore::IntPoint&) const;
  virtual WebCore::IntRect windowToScreen(const WebCore::IntRect&) const;
  virtual PlatformWidget platformWindow() const;
  virtual void scrollRectIntoView(const WebCore::IntRect&,
                                  const WebCore::ScrollView*) const;

  // WebCore::FramelessScrollViewClient methods:
  virtual void popupClosed(WebCore::FramelessScrollView* popup_view);

  // TODO(darin): Figure out what happens to these methods.
#if 0
  // WebCore::WidgetClientWin
  virtual const SkBitmap* getPreloadedResourceBitmap(int resource_id);
  virtual void onScrollPositionChanged(WebCore::Widget* widget);
  virtual bool isHidden();
#endif

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

