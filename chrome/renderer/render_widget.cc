// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/render_widget.h"

#include "base/gfx/point.h"
#include "base/gfx/size.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/scoped_ptr.h"
#include "build/build_config.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/transport_dib.h"
#include "chrome/renderer/render_process.h"
#include "skia/ext/platform_canvas.h"
#include "third_party/skia/include/core/SkShader.h"
#include "webkit/api/public/WebCursorInfo.h"
#include "webkit/api/public/WebRect.h"
#include "webkit/api/public/WebScreenInfo.h"
#include "webkit/api/public/WebSize.h"

#if defined(OS_POSIX)
#include "third_party/skia/include/core/SkPixelRef.h"
#include "third_party/skia/include/core/SkMallocPixelRef.h"
#endif  // defined(OS_POSIX)

#include "webkit/glue/webtextdirection.h"
#include "webkit/glue/webwidget.h"

using WebKit::WebCursorInfo;
using WebKit::WebInputEvent;
using WebKit::WebRect;
using WebKit::WebScreenInfo;
using WebKit::WebSize;

RenderWidget::RenderWidget(RenderThreadBase* render_thread, bool activatable)
    : routing_id_(MSG_ROUTING_NONE),
      webwidget_(NULL),
      opener_id_(MSG_ROUTING_NONE),
      render_thread_(render_thread),
      host_window_(NULL),
      current_paint_buf_(NULL),
      current_scroll_buf_(NULL),
      next_paint_flags_(0),
      paint_reply_pending_(false),
      did_show_(false),
      is_hidden_(false),
      needs_repainting_on_restore_(false),
      has_focus_(false),
      closing_(false),
      ime_is_active_(false),
      ime_control_enable_ime_(true),
      ime_control_x_(-1),
      ime_control_y_(-1),
      ime_control_new_state_(false),
      ime_control_updated_(false),
      ime_control_busy_(false),
      activatable_(activatable),
      pending_window_rect_count_(0) {
  RenderProcess::current()->AddRefProcess();
  DCHECK(render_thread_);
}

RenderWidget::~RenderWidget() {
  DCHECK(!webwidget_) << "Leaking our WebWidget!";
  if (current_paint_buf_) {
    RenderProcess::current()->ReleaseTransportDIB(current_paint_buf_);
    current_paint_buf_ = NULL;
  }
  if (current_scroll_buf_) {
    RenderProcess::current()->ReleaseTransportDIB(current_scroll_buf_);
    current_scroll_buf_ = NULL;
  }
  RenderProcess::current()->ReleaseProcess();
}

/*static*/
RenderWidget* RenderWidget::Create(int32 opener_id,
                                   RenderThreadBase* render_thread,
                                   bool activatable) {
  DCHECK(opener_id != MSG_ROUTING_NONE);
  scoped_refptr<RenderWidget> widget = new RenderWidget(render_thread,
                                                        activatable);
  widget->Init(opener_id);  // adds reference
  return widget;
}

void RenderWidget::Init(int32 opener_id) {
  DCHECK(!webwidget_);

  if (opener_id != MSG_ROUTING_NONE)
    opener_id_ = opener_id;

  webwidget_ = WebWidget::Create(this);

  bool result = render_thread_->Send(
      new ViewHostMsg_CreateWidget(opener_id, activatable_, &routing_id_));
  if (result) {
    render_thread_->AddRoute(routing_id_, this);
    // Take a reference on behalf of the RenderThread.  This will be balanced
    // when we receive ViewMsg_Close.
    AddRef();
  } else {
    DCHECK(false);
  }
}

// This is used to complete pending inits and non-pending inits. For non-
// pending cases, the parent will be the same as the current parent. This
// indicates we do not need to reparent or anything.
void RenderWidget::CompleteInit(gfx::NativeViewId parent_hwnd) {
  DCHECK(routing_id_ != MSG_ROUTING_NONE);

  host_window_ = parent_hwnd;

  Send(new ViewHostMsg_RenderViewReady(routing_id_));
}

IPC_DEFINE_MESSAGE_MAP(RenderWidget)
  IPC_MESSAGE_HANDLER(ViewMsg_Close, OnClose)
  IPC_MESSAGE_HANDLER(ViewMsg_CreatingNew_ACK, OnCreatingNewAck)
  IPC_MESSAGE_HANDLER(ViewMsg_Resize, OnResize)
  IPC_MESSAGE_HANDLER(ViewMsg_WasHidden, OnWasHidden)
  IPC_MESSAGE_HANDLER(ViewMsg_WasRestored, OnWasRestored)
  IPC_MESSAGE_HANDLER(ViewMsg_PaintRect_ACK, OnPaintRectAck)
  IPC_MESSAGE_HANDLER(ViewMsg_ScrollRect_ACK, OnScrollRectAck)
  IPC_MESSAGE_HANDLER(ViewMsg_HandleInputEvent, OnHandleInputEvent)
  IPC_MESSAGE_HANDLER(ViewMsg_MouseCaptureLost, OnMouseCaptureLost)
  IPC_MESSAGE_HANDLER(ViewMsg_SetFocus, OnSetFocus)
  IPC_MESSAGE_HANDLER(ViewMsg_ImeSetInputMode, OnImeSetInputMode)
  IPC_MESSAGE_HANDLER(ViewMsg_ImeSetComposition, OnImeSetComposition)
  IPC_MESSAGE_HANDLER(ViewMsg_Repaint, OnMsgRepaint)
  IPC_MESSAGE_HANDLER(ViewMsg_SetTextDirection, OnSetTextDirection)
  IPC_MESSAGE_HANDLER(ViewMsg_Move_ACK, OnRequestMoveAck)
  IPC_MESSAGE_UNHANDLED_ERROR()
IPC_END_MESSAGE_MAP()

bool RenderWidget::Send(IPC::Message* message) {
  // Don't send any messages after the browser has told us to close.
  if (closing_) {
    delete message;
    return false;
  }

  // If given a messsage without a routing ID, then assign our routing ID.
  if (message->routing_id() == MSG_ROUTING_NONE)
    message->set_routing_id(routing_id_);

  return render_thread_->Send(message);
}

// Got a response from the browser after the renderer decided to create a new
// view.
void RenderWidget::OnCreatingNewAck(gfx::NativeViewId parent) {
  DCHECK(routing_id_ != MSG_ROUTING_NONE);

  CompleteInit(parent);
}

void RenderWidget::OnClose() {
  if (closing_)
    return;
  closing_ = true;

  // Browser correspondence is no longer needed at this point.
  if (routing_id_ != MSG_ROUTING_NONE)
    render_thread_->RemoveRoute(routing_id_);

  // If there is a Send call on the stack, then it could be dangerous to close
  // now.  Post a task that only gets invoked when there are no nested message
  // loops.
  MessageLoop::current()->PostNonNestableTask(FROM_HERE,
      NewRunnableMethod(this, &RenderWidget::Close));

  // Balances the AddRef taken when we called AddRoute.
  Release();
}

void RenderWidget::OnResize(const gfx::Size& new_size,
                            const gfx::Rect& resizer_rect) {
  // During shutdown we can just ignore this message.
  if (!webwidget_)
    return;

  // Remember the rect where the resize corner will be drawn.
  resizer_rect_ = resizer_rect;

  // TODO(darin): We should not need to reset this here.
  is_hidden_ = false;
  needs_repainting_on_restore_ = false;

  // We shouldn't be asked to resize to our current size.
  DCHECK(size_ != new_size);
  size_ = new_size;

  // We should not be sent a Resize message if we have not ACK'd the previous
  DCHECK(!next_paint_is_resize_ack());

  // When resizing, we want to wait to paint before ACK'ing the resize.  This
  // ensures that we only resize as fast as we can paint.  We only need to send
  // an ACK if we are resized to a non-empty rect.
  webwidget_->Resize(new_size);
  if (!new_size.IsEmpty()) {
    DCHECK(!paint_rect_.IsEmpty());

    // This should have caused an invalidation of the entire view.  The damaged
    // rect could be larger than new_size if we are being made smaller.
    DCHECK_GE(paint_rect_.width(), new_size.width());
    DCHECK_GE(paint_rect_.height(), new_size.height());

    // We will send the Resize_ACK flag once we paint again.
    set_next_paint_is_resize_ack();
  }
}

void RenderWidget::OnWasHidden() {
  // Go into a mode where we stop generating paint and scrolling events.
  is_hidden_ = true;
}

void RenderWidget::OnWasRestored(bool needs_repainting) {
  // During shutdown we can just ignore this message.
  if (!webwidget_)
    return;

  // See OnWasHidden
  is_hidden_ = false;

  if (!needs_repainting && !needs_repainting_on_restore_)
    return;
  needs_repainting_on_restore_ = false;

  // Tag the next paint as a restore ack, which is picked up by DoDeferredPaint
  // when it sends out the next PaintRect message.
  set_next_paint_is_restore_ack();

  // Generate a full repaint.
  DidInvalidateRect(webwidget_, gfx::Rect(size_.width(), size_.height()));
}

void RenderWidget::OnPaintRectAck() {
  DCHECK(paint_reply_pending());
  paint_reply_pending_ = false;
  // If we sent a PaintRect message with a zero-sized bitmap, then
  // we should have no current paint buf.
  if (current_paint_buf_) {
    RenderProcess::current()->ReleaseTransportDIB(current_paint_buf_);
    current_paint_buf_ = NULL;
  }

  // Notify subclasses
  DidPaint();

  // Continue painting if necessary...
  DoDeferredPaint();
}

void RenderWidget::OnRequestMoveAck() {
  DCHECK(pending_window_rect_count_);
  pending_window_rect_count_--;
}

void RenderWidget::OnScrollRectAck() {
  DCHECK(scroll_reply_pending());

  if (current_scroll_buf_) {
    RenderProcess::current()->ReleaseTransportDIB(current_scroll_buf_);
    current_scroll_buf_ = NULL;
  }

  // Continue scrolling if necessary...
  DoDeferredScroll();
}

void RenderWidget::OnHandleInputEvent(const IPC::Message& message) {
  void* iter = NULL;

  const char* data;
  int data_length;
  if (!message.ReadData(&iter, &data, &data_length))
    return;

  const WebInputEvent* input_event =
      reinterpret_cast<const WebInputEvent*>(data);
  bool processed = false;
  if (webwidget_)
    processed = webwidget_->HandleInputEvent(input_event);

  IPC::Message* response = new ViewHostMsg_HandleInputEvent_ACK(routing_id_);
  response->WriteInt(input_event->type);
  response->WriteBool(processed);

  Send(response);
}

void RenderWidget::OnMouseCaptureLost() {
  if (webwidget_)
    webwidget_->MouseCaptureLost();
}

void RenderWidget::OnSetFocus(bool enable) {
  has_focus_ = enable;
  if (webwidget_)
    webwidget_->SetFocus(enable);
  if (enable) {
    // Force to retrieve the state of the focused widget to determine if we
    // should activate IMEs next time when this process calls the UpdateIME()
    // function.
    ime_control_updated_ = true;
    ime_control_new_state_ = true;
  }
}

void RenderWidget::ClearFocus() {
  // We may have got the focus from the browser before this gets processed, in
  // which case we do not want to unfocus ourself.
  if (!has_focus_ && webwidget_)
    webwidget_->SetFocus(false);
}

void RenderWidget::PaintRect(const gfx::Rect& rect,
                             skia::PlatformCanvas* canvas) {

  // Bring the canvas into the coordinate system of the paint rect.
  canvas->translate(static_cast<SkScalar>(-rect.x()),
                    static_cast<SkScalar>(-rect.y()));

  // If there is a custom background, tile it.
  if (!background_.empty()) {
    SkPaint paint;
    SkShader* shader = SkShader::CreateBitmapShader(background_,
                                                    SkShader::kRepeat_TileMode,
                                                    SkShader::kRepeat_TileMode);
    paint.setShader(shader)->unref();
    paint.setXfermodeMode(SkXfermode::kSrcOver_Mode);
    canvas->drawPaint(paint);
  }

  webwidget_->Paint(canvas, rect);

  // Flush to underlying bitmap.  TODO(darin): is this needed?
  canvas->getTopPlatformDevice().accessBitmap(false);
}

void RenderWidget::DoDeferredPaint() {
  if (!webwidget_ || paint_reply_pending() || paint_rect_.IsEmpty())
    return;

  // When we are hidden, we want to suppress painting, but we still need to
  // mark this DoDeferredPaint as complete.
  if (is_hidden_ || size_.IsEmpty()) {
    paint_rect_ = gfx::Rect();
    needs_repainting_on_restore_ = true;
    return;
  }

  // Layout may generate more invalidation...
  webwidget_->Layout();

  // OK, save the current paint_rect to a local since painting may cause more
  // invalidation.  Some WebCore rendering objects only layout when painted.
  gfx::Rect damaged_rect = paint_rect_;
  paint_rect_ = gfx::Rect();

  // Compute a buffer for painting and cache it.
  skia::PlatformCanvas* canvas =
      RenderProcess::current()->GetDrawingCanvas(&current_paint_buf_,
                                                 damaged_rect);
  if (!canvas) {
    NOTREACHED();
    return;
  }

  // We may get back a smaller canvas than we asked for.
  damaged_rect.set_width(canvas->getDevice()->width());
  damaged_rect.set_height(canvas->getDevice()->height());

  PaintRect(damaged_rect, canvas);

  ViewHostMsg_PaintRect_Params params;
  params.bitmap_rect = damaged_rect;
  params.view_size = size_;
  params.plugin_window_moves = plugin_window_moves_;
  params.flags = next_paint_flags_;
  params.bitmap = current_paint_buf_->id();

  delete canvas;

  plugin_window_moves_.clear();

  paint_reply_pending_ = true;
  Send(new ViewHostMsg_PaintRect(routing_id_, params));
  next_paint_flags_ = 0;

  UpdateIME();
}

void RenderWidget::DoDeferredScroll() {
  if (!webwidget_ || scroll_reply_pending() || scroll_rect_.IsEmpty())
    return;

  // When we are hidden, we want to suppress scrolling, but we still need to
  // mark this DoDeferredScroll as complete.
  if (is_hidden_ || size_.IsEmpty()) {
    scroll_rect_ = gfx::Rect();
    needs_repainting_on_restore_ = true;
    return;
  }

  // Layout may generate more invalidation, so we might have to bail on
  // optimized scrolling...
  webwidget_->Layout();

  if (scroll_rect_.IsEmpty())
    return;

  gfx::Rect damaged_rect;

  // Compute the region we will expose by scrolling, and paint that into a
  // shared memory section.
  if (scroll_delta_.x()) {
    int dx = scroll_delta_.x();
    damaged_rect.set_y(scroll_rect_.y());
    damaged_rect.set_height(scroll_rect_.height());
    if (dx > 0) {
      damaged_rect.set_x(scroll_rect_.x());
      damaged_rect.set_width(dx);
    } else {
      damaged_rect.set_x(scroll_rect_.right() + dx);
      damaged_rect.set_width(-dx);
    }
  } else {
    int dy = scroll_delta_.y();
    damaged_rect.set_x(scroll_rect_.x());
    damaged_rect.set_width(scroll_rect_.width());
    if (dy > 0) {
      damaged_rect.set_y(scroll_rect_.y());
      damaged_rect.set_height(dy);
    } else {
      damaged_rect.set_y(scroll_rect_.bottom() + dy);
      damaged_rect.set_height(-dy);
    }
  }

  // In case the scroll offset exceeds the width/height of the scroll rect
  damaged_rect = scroll_rect_.Intersect(damaged_rect);

  skia::PlatformCanvas* canvas =
      RenderProcess::current()->GetDrawingCanvas(&current_scroll_buf_,
                                                 damaged_rect);
  if (!canvas) {
    NOTREACHED();
    return;
  }

  // We may get back a smaller canvas than we asked for.
  damaged_rect.set_width(canvas->getDevice()->width());
  damaged_rect.set_height(canvas->getDevice()->height());

  // Set these parameters before calling Paint, since that could result in
  // further invalidates (uncommon).
  ViewHostMsg_ScrollRect_Params params;
  params.bitmap_rect = damaged_rect;
  params.dx = scroll_delta_.x();
  params.dy = scroll_delta_.y();
  params.clip_rect = scroll_rect_;
  params.view_size = size_;
  params.plugin_window_moves = plugin_window_moves_;
  params.bitmap = current_scroll_buf_->id();

  plugin_window_moves_.clear();

  // Mark the scroll operation as no longer pending.
  scroll_rect_ = gfx::Rect();

  PaintRect(damaged_rect, canvas);
  Send(new ViewHostMsg_ScrollRect(routing_id_, params));
  delete canvas;
  UpdateIME();
}

///////////////////////////////////////////////////////////////////////////////
// WebWidgetDelegate

gfx::NativeViewId RenderWidget::GetContainingView(WebWidget* webwidget) {
  return host_window_;
}

void RenderWidget::DidInvalidateRect(WebWidget* webwidget,
                                     const WebRect& rect) {
  // We only want one pending DoDeferredPaint call at any time...
  bool paint_pending = !paint_rect_.IsEmpty();

  // If this invalidate overlaps with a pending scroll, then we have to
  // downgrade to invalidating the scroll rect.
  if (gfx::Rect(rect).Intersects(scroll_rect_)) {
    paint_rect_ = paint_rect_.Union(scroll_rect_);
    scroll_rect_ = gfx::Rect();
  }

  gfx::Rect view_rect(0, 0, size_.width(), size_.height());
  // TODO(iyengar) Investigate why we have painting issues when
  // we ignore invalid regions outside the view.
  // Ignore invalidates that occur outside the bounds of the view
  // TODO(darin): maybe this should move into the paint code?
  // paint_rect_ = view_rect.Intersect(paint_rect_.Union(rect));
  paint_rect_ = paint_rect_.Union(view_rect.Intersect(rect));

  if (paint_rect_.IsEmpty() || paint_reply_pending() || paint_pending)
    return;

  // Perform painting asynchronously.  This serves two purposes:
  // 1) Ensures that we call WebView::Paint without a bunch of other junk
  //    on the call stack.
  // 2) Allows us to collect more damage rects before painting to help coalesce
  //    the work that we will need to do.
  MessageLoop::current()->PostTask(FROM_HERE, NewRunnableMethod(
      this, &RenderWidget::DoDeferredPaint));
}

void RenderWidget::DidScrollRect(WebWidget* webwidget, int dx, int dy,
                                 const WebRect& clip_rect) {
  if (dx != 0 && dy != 0) {
    // We only support scrolling along one axis at a time.
    DidScrollRect(webwidget, 0, dy, clip_rect);
    dy = 0;
  }

  bool intersects_with_painting = paint_rect_.Intersects(clip_rect);

  // If we already have a pending scroll operation or if this scroll operation
  // intersects the existing paint region, then just failover to invalidating.
  if (!scroll_rect_.IsEmpty() || intersects_with_painting) {
    if (!intersects_with_painting && scroll_rect_ == gfx::Rect(clip_rect)) {
      // OK, we can just update the scroll delta (requires same scrolling axis)
      if (!dx && !scroll_delta_.x()) {
        scroll_delta_.set_y(scroll_delta_.y() + dy);
        return;
      }
      if (!dy && !scroll_delta_.y()) {
        scroll_delta_.set_x(scroll_delta_.x() + dx);
        return;
      }
    }
    DidInvalidateRect(webwidget_, scroll_rect_);
    DCHECK(scroll_rect_.IsEmpty());
    DidInvalidateRect(webwidget_, clip_rect);
    return;
  }

  // We only want one pending DoDeferredScroll call at any time...
  bool scroll_pending = !scroll_rect_.IsEmpty();

  scroll_rect_ = clip_rect;
  scroll_delta_.SetPoint(dx, dy);

  if (scroll_pending)
    return;

  // Perform scrolling asynchronously since we need to call WebView::Paint
  MessageLoop::current()->PostTask(FROM_HERE, NewRunnableMethod(
      this, &RenderWidget::DoDeferredScroll));
}

void RenderWidget::SetCursor(WebWidget* webwidget,
                             const WebCursorInfo& cursor_info) {
  // TODO(darin): Eliminate this temporary.
  WebCursor cursor(cursor_info);

  // Only send a SetCursor message if we need to make a change.
  if (!current_cursor_.IsEqual(cursor)) {
    current_cursor_ = cursor;
    Send(new ViewHostMsg_SetCursor(routing_id_, cursor));
  }
}

// We are supposed to get a single call to Show for a newly created RenderWidget
// that was created via RenderWidget::CreateWebView.  So, we wait until this
// point to dispatch the ShowWidget message.
//
// This method provides us with the information about how to display the newly
// created RenderWidget (i.e., as a constrained popup or as a new tab).
//
void RenderWidget::Show(WebWidget* webwidget,
                        WindowOpenDisposition disposition) {
  DCHECK(!did_show_) << "received extraneous Show call";
  DCHECK(routing_id_ != MSG_ROUTING_NONE);
  DCHECK(opener_id_ != MSG_ROUTING_NONE);

  if (!did_show_) {
    did_show_ = true;
    // NOTE: initial_pos_ may still have its default values at this point, but
    // that's okay.  It'll be ignored if as_popup is false, or the browser
    // process will impose a default position otherwise.
    render_thread_->Send(new ViewHostMsg_ShowWidget(
        opener_id_, routing_id_, initial_pos_));
    SetPendingWindowRect(initial_pos_);
  }
}

void RenderWidget::ShowAsPopupWithItems(WebWidget* webwidget,
                                        const WebRect& bounds,
                                        int item_height,
                                        int selected_index,
                                        const std::vector<WebMenuItem>& items) {
  ViewHostMsg_ShowPopup_Params params;
  params.bounds = bounds;
  params.item_height = item_height;
  params.selected_item = selected_index;
  params.popup_items = items;

  Send(new ViewHostMsg_ShowPopup(routing_id_, params));
}

void RenderWidget::Focus(WebWidget* webwidget) {
  // Prevent the widget from stealing the focus if it does not have focus
  // already.  We do this by explicitely setting the focus to false again.
  // We only let the browser focus the renderer.
  if (!has_focus_ && webwidget_) {
    MessageLoop::current()->PostTask(FROM_HERE,
        NewRunnableMethod(this, &RenderWidget::ClearFocus));
  }
}

void RenderWidget::Blur(WebWidget* webwidget) {
  Send(new ViewHostMsg_Blur(routing_id_));
}

void RenderWidget::DoDeferredClose() {
  Send(new ViewHostMsg_Close(routing_id_));
}

void RenderWidget::CloseWidgetSoon(WebWidget* webwidget) {
  // If a page calls window.close() twice, we'll end up here twice, but that's
  // OK.  It is safe to send multiple Close messages.

  // Ask the RenderWidgetHost to initiate close.  We could be called from deep
  // in Javascript.  If we ask the RendwerWidgetHost to close now, the window
  // could be closed before the JS finishes executing.  So instead, post a
  // message back to the message loop, which won't run until the JS is
  // complete, and then the Close message can be sent.
  MessageLoop::current()->PostNonNestableTask(FROM_HERE, NewRunnableMethod(
      this, &RenderWidget::DoDeferredClose));
}

void RenderWidget::GenerateFullRepaint() {
  DidInvalidateRect(webwidget_, gfx::Rect(size_.width(), size_.height()));
}

void RenderWidget::Close() {
  if (webwidget_) {
    webwidget_->Close();
    webwidget_ = NULL;
  }
}

void RenderWidget::GetWindowRect(WebWidget* webwidget, WebRect* result) {
  if (pending_window_rect_count_) {
    *result = pending_window_rect_;
    return;
  }

  gfx::Rect rect;
  Send(new ViewHostMsg_GetWindowRect(routing_id_, host_window_, &rect));
  *result = rect;
}

void RenderWidget::SetWindowRect(WebWidget* webwidget, const WebRect& pos) {
  if (did_show_) {
    Send(new ViewHostMsg_RequestMove(routing_id_, pos));
    SetPendingWindowRect(pos);
  } else {
    initial_pos_ = pos;
  }
}

void RenderWidget::SetPendingWindowRect(const WebRect& rect) {
  pending_window_rect_ = rect;
  pending_window_rect_count_++;
}

void RenderWidget::GetRootWindowRect(WebWidget* webwidget, WebRect* result) {
  if (pending_window_rect_count_) {
    // NOTE(mbelshe): If there is a pending_window_rect_, then getting
    // the RootWindowRect is probably going to return wrong results since the
    // browser may not have processed the Move yet.  There isn't really anything
    // good to do in this case, and it shouldn't happen - since this size is
    // only really needed for windowToScreen, which is only used for Popups.
    *result = pending_window_rect_;
    return;
  }

  gfx::Rect rect;
  Send(new ViewHostMsg_GetRootWindowRect(routing_id_, host_window_, &rect));
  *result = rect;
}

void RenderWidget::GetRootWindowResizerRect(WebWidget* webwidget,
                                            WebRect* rect) {
  *rect = resizer_rect_;
}

void RenderWidget::OnImeSetInputMode(bool is_active) {
  // To prevent this renderer process from sending unnecessary IPC messages to
  // a browser process, we permit the renderer process to send IPC messages
  // only during the IME attached to the browser process is active.
  ime_is_active_ = is_active;
}

void RenderWidget::OnImeSetComposition(int string_type,
                                       int cursor_position,
                                       int target_start, int target_end,
                                       const std::wstring& ime_string) {
  if (webwidget_) {
    ime_control_busy_ = true;
    webwidget_->ImeSetComposition(string_type, cursor_position,
                                  target_start, target_end,
                                  ime_string);
    ime_control_busy_ = false;
  }
}

void RenderWidget::OnMsgRepaint(const gfx::Size& size_to_paint) {
  // During shutdown we can just ignore this message.
  if (!webwidget_)
    return;

  set_next_paint_is_repaint_ack();
  gfx::Rect repaint_rect(size_to_paint.width(), size_to_paint.height());
  DidInvalidateRect(webwidget_, repaint_rect);
}

void RenderWidget::OnSetTextDirection(int direction) {
  if (!webwidget_)
    return;

  WebTextDirection new_direction = static_cast<WebTextDirection>(direction);
  if (new_direction == WEB_TEXT_DIRECTION_DEFAULT ||
      new_direction == WEB_TEXT_DIRECTION_LTR ||
      new_direction == WEB_TEXT_DIRECTION_RTL) {
    webwidget_->SetTextDirection(new_direction);
  } else {
    NOTREACHED();
  }
}

void RenderWidget::SetBackground(const SkBitmap& background) {
  background_ = background;
  // Generate a full repaint.
  DidInvalidateRect(webwidget_, gfx::Rect(size_.width(), size_.height()));
}

bool RenderWidget::next_paint_is_resize_ack() const {
  return ViewHostMsg_PaintRect_Flags::is_resize_ack(next_paint_flags_);
}

bool RenderWidget::next_paint_is_restore_ack() const {
  return ViewHostMsg_PaintRect_Flags::is_restore_ack(next_paint_flags_);
}

void RenderWidget::set_next_paint_is_resize_ack() {
  next_paint_flags_ |= ViewHostMsg_PaintRect_Flags::IS_RESIZE_ACK;
}

void RenderWidget::set_next_paint_is_restore_ack() {
  next_paint_flags_ |= ViewHostMsg_PaintRect_Flags::IS_RESTORE_ACK;
}

void RenderWidget::set_next_paint_is_repaint_ack() {
  next_paint_flags_ |= ViewHostMsg_PaintRect_Flags::IS_REPAINT_ACK;
}

void RenderWidget::UpdateIME() {
  // If a browser process does not have IMEs, its IMEs are not active, or there
  // are not any attached widgets.
  // a renderer process does not have to retrieve information of the focused
  // control or send notification messages to a browser process.
  if (!ime_is_active_) {
    return;
  }
  // Retrieve the caret position from the focused widget and verify we should
  // enabled IMEs attached to the browser process.
  bool enable_ime = false;
  WebRect caret_rect;
  if (!webwidget_ ||
      !webwidget_->ImeUpdateStatus(&enable_ime, &caret_rect)) {
    // There are not any editable widgets attached to this process.
    // We should disable the IME to prevent it from sending CJK strings to
    // non-editable widgets.
    ime_control_updated_ = true;
    ime_control_new_state_ = false;
  }
  if (ime_control_new_state_ != enable_ime) {
    ime_control_updated_ = true;
    ime_control_new_state_ = enable_ime;
  }
  if (ime_control_updated_) {
    // The input focus has been changed.
    // Compare the current state with the updated state and choose actions.
    if (ime_control_enable_ime_) {
      if (ime_control_new_state_) {
        // Case 1: a text input -> another text input
        // Complete the current composition and notify the caret position.
        Send(new ViewHostMsg_ImeUpdateStatus(routing_id(),
                                             IME_COMPLETE_COMPOSITION,
                                             caret_rect));
      } else {
        // Case 2: a text input -> a password input (or a static control)
        // Complete the current composition and disable the IME.
        Send(new ViewHostMsg_ImeUpdateStatus(routing_id(), IME_DISABLE,
                                             caret_rect));
      }
    } else {
      if (ime_control_new_state_) {
        // Case 3: a password input (or a static control) -> a text input
        // Enable the IME and notify the caret position.
        Send(new ViewHostMsg_ImeUpdateStatus(routing_id(),
                                             IME_COMPLETE_COMPOSITION,
                                             caret_rect));
      } else {
        // Case 4: a password input (or a static contol) -> another password
        //         input (or another static control).
        // The IME has been already disabled and we don't have to do anything.
      }
    }
  } else {
    // The input focus is not changed.
    // Notify the caret position to a browser process only if it is changed.
    if (ime_control_enable_ime_) {
      if (caret_rect.x != ime_control_x_ ||
          caret_rect.y != ime_control_y_) {
        Send(new ViewHostMsg_ImeUpdateStatus(routing_id(), IME_MOVE_WINDOWS,
                                             caret_rect));
      }
    }
  }
  // Save the updated IME status to prevent from sending the same IPC messages.
  ime_control_updated_ = false;
  ime_control_enable_ime_ = ime_control_new_state_;
  ime_control_x_ = caret_rect.x;
  ime_control_y_ = caret_rect.y;
}

void RenderWidget::DidMove(WebWidget* webwidget,
                           const WebPluginGeometry& move) {
  size_t i = 0;
  for (; i < plugin_window_moves_.size(); ++i) {
    if (plugin_window_moves_[i].window == move.window) {
      plugin_window_moves_[i] = move;
      break;
    }
  }

  if (i == plugin_window_moves_.size())
    plugin_window_moves_.push_back(move);
}

WebScreenInfo RenderWidget::GetScreenInfo(WebWidget* webwidget) {
  WebScreenInfo results;
  Send(new ViewHostMsg_GetScreenInfo(routing_id_, host_window_, &results));
  return results;
}
