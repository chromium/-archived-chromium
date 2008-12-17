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
#include "base/gfx/rect.h"
#include "base/logging.h"
#include "skia/ext/platform_canvas.h"
#include "webkit/glue/event_conversion.h"
#include "webkit/glue/glue_util.h"
#include "webkit/glue/webinputevent.h"
#include "webkit/glue/webwidget_delegate.h"
#include "webkit/glue/webwidget_impl.h"

using namespace WebCore;

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
  last_mouse_position_.SetPoint(-1, -1);
}

WebWidgetImpl::~WebWidgetImpl() {
  if (widget_)
    widget_->setClient(NULL);
}

void WebWidgetImpl::Init(WebCore::FramelessScrollView* widget,
                         const gfx::Rect& bounds) {
  widget_ = widget;
  widget_->setClient(this);

  if (delegate_) {
    delegate_->SetWindowRect(this, bounds);
    delegate_->Show(this, WindowOpenDisposition());
  }
}

void WebWidgetImpl::MouseMove(const WebMouseEvent& event) {
  // don't send mouse move messages if the mouse hasn't moved.
  if (event.x != last_mouse_position_.x() ||
      event.y != last_mouse_position_.y()) {
    last_mouse_position_.SetPoint(event.x, event.y);

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
}

void WebWidgetImpl::Resize(const gfx::Size& new_size) {
  if (size_ == new_size)
    return;
  size_ = new_size;

  if (widget_) {
    IntRect new_geometry(0, 0, size_.width(), size_.height());
    widget_->setFrameRect(new_geometry);
  }

  if (delegate_) {
    gfx::Rect damaged_rect(0, 0, size_.width(), size_.height());
    delegate_->DidInvalidateRect(this, damaged_rect);
  }
}

void WebWidgetImpl::Layout() {
}

void WebWidgetImpl::Paint(skia::PlatformCanvas* canvas, const gfx::Rect& rect) {
  if (!widget_)
    return;

  if (!rect.IsEmpty()) {
#if defined(OS_MACOSX)
    CGContextRef context = canvas->getTopPlatformDevice().GetBitmapContext();
    GraphicsContext gc(context);
#else
    PlatformContextSkia context(canvas);
    // PlatformGraphicsContext is actually a pointer to PlatformContextSkia.
    GraphicsContext gc(reinterpret_cast<PlatformGraphicsContext*>(&context));
#endif

    IntRect dirty_rect(rect.x(), rect.y(), rect.width(), rect.height());

    widget_->paint(&gc, dirty_rect);
  }
}

bool WebWidgetImpl::HandleInputEvent(const WebInputEvent* input_event) {
  if (!widget_)
    return false;

  // TODO (jcampan): WebKit seems to always return false on mouse events
  // methods. For now we'll assume it has processed them (as we are only
  // interested in whether keyboard events are processed).
  switch (input_event->type) {
    case WebInputEvent::MOUSE_MOVE:
      MouseMove(*static_cast<const WebMouseEvent*>(input_event));
      return true;

    case WebInputEvent::MOUSE_LEAVE:
      MouseLeave(*static_cast<const WebMouseEvent*>(input_event));
      return true;

    case WebInputEvent::MOUSE_WHEEL:
      MouseWheel(*static_cast<const WebMouseWheelEvent*>(input_event));
      return true;

    case WebInputEvent::MOUSE_DOWN:
    case WebInputEvent::MOUSE_DOUBLE_CLICK:
      MouseDown(*static_cast<const WebMouseEvent*>(input_event));
      return true;

    case WebInputEvent::MOUSE_UP:
      MouseUp(*static_cast<const WebMouseEvent*>(input_event));
      return true;

    case WebInputEvent::KEY_DOWN:
    case WebInputEvent::KEY_UP:
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
                                    const void** node,
                                    gfx::Rect* caret_rect) {
  return false;
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
    delegate_->DidInvalidateRect(this, webkit_glue::FromIntRect(paint_rect));
}

void WebWidgetImpl::scroll(const WebCore::IntSize& scroll_delta,
                           const WebCore::IntRect& scroll_rect,
                           const WebCore::IntRect& clip_rect) {
  if (delegate_) {
    int dx = scroll_delta.width();
    int dy = scroll_delta.height();
    delegate_->DidScrollRect(this, dx, dy, webkit_glue::FromIntRect(clip_rect));
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
  if (!delegate_)
    return NULL;
  return delegate_->GetContainingWindow(const_cast<WebWidgetImpl*>(this));
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

//-----------------------------------------------------------------------------
// WebCore::WidgetClientWin

// TODO(darin): Figure out what happens to these methods.
#if 0
const SkBitmap* WebWidgetImpl::getPreloadedResourceBitmap(int resource_id) {
  return NULL;
}

void WebWidgetImpl::onScrollPositionChanged(Widget* widget) {
}

bool WebWidgetImpl::isHidden() {
  if (!delegate_)
    return true;

  return delegate_->IsHidden();
}
#endif
