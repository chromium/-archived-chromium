// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "base/compiler_specific.h"

MSVC_PUSH_WARNING_LEVEL(0);
#include "Cursor.h"
#include "FramelessScrollView.h"
#include "FrameView.h"
#include "IntRect.h"
#include "PlatformContextSkia.h"
#include "PlatformKeyboardEvent.h"
#include "PlatformMouseEvent.h"
#include "PlatformWheelEvent.h"
#include "SkiaUtils.h"
MSVC_POP_WARNING();

#undef LOG
#include "base/logging.h"
#include "skia/ext/platform_canvas.h"
#include "webkit/api/public/WebInputEvent.h"
#include "webkit/api/public/WebRect.h"
#include "webkit/glue/event_conversion.h"
#include "webkit/glue/glue_util.h"
#include "webkit/glue/webwidget_delegate.h"
#include "webkit/glue/webwidget_impl.h"

using namespace WebCore;

using WebKit::WebInputEvent;
using WebKit::WebKeyboardEvent;
using WebKit::WebMouseEvent;
using WebKit::WebMouseWheelEvent;
using WebKit::WebPoint;
using WebKit::WebRect;
using WebKit::WebSize;

// WebWidget ----------------------------------------------------------------

/*static*/
WebWidget* WebWidget::Create(WebWidgetDelegate* delegate) {
  WebWidgetImpl* instance = new WebWidgetImpl(delegate);
  instance->AddRef();
  return instance;
}

WebWidgetImpl::WebWidgetImpl(WebWidgetDelegate* delegate)
    : delegate_(delegate),
      widget_(NULL) {
  // set to impossible point so we always get the first mouse pos
  last_mouse_position_ = WebPoint(-1, -1);
}

WebWidgetImpl::~WebWidgetImpl() {
  if (widget_)
    widget_->setClient(NULL);
}

void WebWidgetImpl::Init(WebCore::FramelessScrollView* widget,
                         const WebRect& bounds) {
  widget_ = widget;
  widget_->setClient(this);

  if (delegate_) {
    delegate_->SetWindowRect(this, bounds);
    delegate_->Show(this, WindowOpenDisposition());
  }
}

void WebWidgetImpl::InitWithItems(WebCore::FramelessScrollView* widget,
                                  const WebRect& bounds,
                                  int item_height,
                                  int selected_index,
                                  const std::vector<WebMenuItem>& items) {
  widget_ = widget;
  widget_->setClient(this);

  if (delegate_) {
    delegate_->ShowAsPopupWithItems(this, bounds, item_height,
                                    selected_index, items);
  }
}

void WebWidgetImpl::MouseMove(const WebMouseEvent& event) {
  // don't send mouse move messages if the mouse hasn't moved.
  if (event.x != last_mouse_position_.x ||
      event.y != last_mouse_position_.y) {
    last_mouse_position_ = WebPoint(event.x, event.y);
    widget_->handleMouseMoveEvent(MakePlatformMouseEvent(widget_, event));
  }
}

void WebWidgetImpl::MouseLeave(const WebMouseEvent& event) {
  widget_->handleMouseMoveEvent(MakePlatformMouseEvent(widget_, event));
}

void WebWidgetImpl::MouseDown(const WebMouseEvent& event) {
  widget_->handleMouseDownEvent(MakePlatformMouseEvent(widget_, event));
}

void WebWidgetImpl::MouseUp(const WebMouseEvent& event) {
  MouseCaptureLost();
  widget_->handleMouseReleaseEvent(MakePlatformMouseEvent(widget_, event));
}

void WebWidgetImpl::MouseWheel(const WebMouseWheelEvent& event) {
  widget_->handleWheelEvent(MakePlatformWheelEvent(widget_, event));
}

bool WebWidgetImpl::KeyEvent(const WebKeyboardEvent& event) {
  return widget_->handleKeyEvent(MakePlatformKeyboardEvent(event));
}

// WebWidget -------------------------------------------------------------------

void WebWidgetImpl::Close() {
  if (widget_)
    widget_->hide();

  delegate_ = NULL;

  Release();  // Balances AddRef from WebWidget::Create
}

void WebWidgetImpl::Resize(const WebSize& new_size) {
  if (size_ == new_size)
    return;
  size_ = new_size;

  if (widget_) {
    IntRect new_geometry(0, 0, size_.width, size_.height);
    widget_->setFrameRect(new_geometry);
  }

  if (delegate_) {
    WebRect damaged_rect(0, 0, size_.width, size_.height);
    delegate_->DidInvalidateRect(this, damaged_rect);
  }
}

void WebWidgetImpl::Layout() {
}

void WebWidgetImpl::Paint(skia::PlatformCanvas* canvas, const WebRect& rect) {
  if (!widget_)
    return;

  if (!rect.isEmpty()) {
#if defined(OS_MACOSX)
    CGContextRef context = canvas->getTopPlatformDevice().GetBitmapContext();
    GraphicsContext gc(context);
#else
    PlatformContextSkia context(canvas);
    // PlatformGraphicsContext is actually a pointer to PlatformContextSkia.
    GraphicsContext gc(reinterpret_cast<PlatformGraphicsContext*>(&context));
#endif

    widget_->paint(&gc, webkit_glue::WebRectToIntRect(rect));
  }
}

bool WebWidgetImpl::HandleInputEvent(const WebInputEvent* input_event) {
  if (!widget_)
    return false;

  // TODO (jcampan): WebKit seems to always return false on mouse events
  // methods. For now we'll assume it has processed them (as we are only
  // interested in whether keyboard events are processed).
  switch (input_event->type) {
    case WebInputEvent::MouseMove:
      MouseMove(*static_cast<const WebMouseEvent*>(input_event));
      return true;

    case WebInputEvent::MouseLeave:
      MouseLeave(*static_cast<const WebMouseEvent*>(input_event));
      return true;

    case WebInputEvent::MouseWheel:
      MouseWheel(*static_cast<const WebMouseWheelEvent*>(input_event));
      return true;

    case WebInputEvent::MouseDown:
      MouseDown(*static_cast<const WebMouseEvent*>(input_event));
      return true;

    case WebInputEvent::MouseUp:
      MouseUp(*static_cast<const WebMouseEvent*>(input_event));
      return true;

    // In Windows, RawKeyDown only has information about the physical key, but
    // for "selection", we need the information about the character the key
    // translated into. For English, the physical key value and the character
    // value are the same, hence, "selection" works for English. But for other
    // languages, such as Hebrew, the character value is different from the
    // physical key value. Thus, without accepting Char event type which
    // contains the key's character value, the "selection" won't work for
    // non-English languages, such as Hebrew.
    case WebInputEvent::RawKeyDown:
    case WebInputEvent::KeyDown:
    case WebInputEvent::KeyUp:
    case WebInputEvent::Char:
      return KeyEvent(*static_cast<const WebKeyboardEvent*>(input_event));

    default:
      break;
  }
  return false;
}

void WebWidgetImpl::MouseCaptureLost() {
}

void WebWidgetImpl::SetFocus(bool enable) {
}

bool WebWidgetImpl::ImeSetComposition(int string_type,
                                      int cursor_position,
                                      int target_start,
                                      int target_end,
                                      const std::wstring& ime_string) {
  return false;
}

bool WebWidgetImpl::ImeUpdateStatus(bool* enable_ime,
                                    WebRect* caret_rect) {
  return false;
}

void WebWidgetImpl::SetTextDirection(WebTextDirection direction) {
}

//-----------------------------------------------------------------------------
// WebCore::HostWindow

void WebWidgetImpl::repaint(const WebCore::IntRect& paint_rect,
                            bool content_changed,
                            bool immediate,
                            bool repaint_content_only) {
  // Ignore spurious calls.
  if (!content_changed || paint_rect.isEmpty())
    return;
  if (delegate_)
    delegate_->DidInvalidateRect(this,
                                 webkit_glue::IntRectToWebRect(paint_rect));
}

void WebWidgetImpl::scroll(const WebCore::IntSize& scroll_delta,
                           const WebCore::IntRect& scroll_rect,
                           const WebCore::IntRect& clip_rect) {
  if (delegate_) {
    int dx = scroll_delta.width();
    int dy = scroll_delta.height();
    delegate_->DidScrollRect(this, dx, dy,
                             webkit_glue::IntRectToWebRect(clip_rect));
  }
}

WebCore::IntPoint WebWidgetImpl::screenToWindow(
    const WebCore::IntPoint& point) const {
  NOTIMPLEMENTED();
  return WebCore::IntPoint();
}

WebCore::IntRect WebWidgetImpl::windowToScreen(
    const WebCore::IntRect& rect) const {
  NOTIMPLEMENTED();
  return WebCore::IntRect();
}

PlatformWidget WebWidgetImpl::platformWindow() const {
  return NULL;
}

void WebWidgetImpl::scrollRectIntoView(
    const WebCore::IntRect&, const WebCore::ScrollView*) const {
  // Nothing to be done here since we do not have the concept of a container
  // that implements its own scrolling.
}

//-----------------------------------------------------------------------------
// WebCore::FramelessScrollViewClient

void WebWidgetImpl::popupClosed(WebCore::FramelessScrollView* widget) {
  DCHECK(widget == widget_);
  if (widget_) {
    widget_->setClient(NULL);
    widget_ = NULL;
  }
  delegate_->CloseWidgetSoon(this);
}
