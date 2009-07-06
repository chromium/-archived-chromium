// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_host/render_widget_host.h"

#include "base/histogram.h"
#include "base/message_loop.h"
#include "base/keyboard_codes.h"
#include "chrome/browser/renderer_host/backing_store.h"
#include "chrome/browser/renderer_host/backing_store_manager.h"
#include "chrome/browser/renderer_host/render_process_host.h"
#include "chrome/browser/renderer_host/render_widget_helper.h"
#include "chrome/browser/renderer_host/render_widget_host_painting_observer.h"
#include "chrome/browser/renderer_host/render_widget_host_view.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/render_messages.h"
#include "views/view.h"
#include "webkit/glue/webcursor.h"
#include "webkit/glue/webtextdirection.h"

#if defined(OS_WIN)
#include "base/gfx/gdi_util.h"
#include "chrome/app/chrome_dll_resource.h"
#endif  // defined(OS_WIN)

using base::Time;
using base::TimeDelta;
using base::TimeTicks;

using WebKit::WebInputEvent;
using WebKit::WebKeyboardEvent;
using WebKit::WebMouseEvent;
using WebKit::WebMouseWheelEvent;

// How long to (synchronously) wait for the renderer to respond with a
// PaintRect message, when our backing-store is invalid, before giving up and
// returning a null or incorrectly sized backing-store from GetBackingStore.
// This timeout impacts the "choppiness" of our window resize perf.
static const int kPaintMsgTimeoutMS = 40;

// How long to wait before we consider a renderer hung.
static const int kHungRendererDelayMs = 20000;

///////////////////////////////////////////////////////////////////////////////
// RenderWidgetHost

RenderWidgetHost::RenderWidgetHost(RenderProcessHost* process,
                                   int routing_id)
    : renderer_initialized_(false),
      view_(NULL),
      process_(process),
      painting_observer_(NULL),
      routing_id_(routing_id),
      is_loading_(false),
      is_hidden_(false),
      repaint_ack_pending_(false),
      resize_ack_pending_(false),
      mouse_move_pending_(false),
      needs_repainting_on_restore_(false),
      is_unresponsive_(false),
      in_get_backing_store_(false),
      view_being_painted_(false),
      text_direction_updated_(false),
      text_direction_(WEB_TEXT_DIRECTION_LTR),
      text_direction_canceled_(false) {
  if (routing_id_ == MSG_ROUTING_NONE)
    routing_id_ = process_->GetNextRoutingID();

  process_->Attach(this, routing_id_);
  // Because the widget initializes as is_hidden_ == false,
  // tell the process host that we're alive.
  process_->WidgetRestored();
}

RenderWidgetHost::~RenderWidgetHost() {
  // Clear our current or cached backing store if either remains.
  BackingStoreManager::RemoveBackingStore(this);

  process_->Release(routing_id_);
}

gfx::NativeViewId RenderWidgetHost::GetNativeViewId() {
  if (view_)
    return gfx::IdFromNativeView(view_->GetNativeView());
  return NULL;
}

void RenderWidgetHost::Init() {
  DCHECK(process_->HasConnection());

  renderer_initialized_ = true;

  // Send the ack along with the information on placement.
  Send(new ViewMsg_CreatingNew_ACK(routing_id_, GetNativeViewId()));
  WasResized();
}

void RenderWidgetHost::Shutdown() {
  if (process_->HasConnection()) {
    // Tell the renderer object to close.
    process_->ReportExpectingClose(routing_id_);
    bool rv = Send(new ViewMsg_Close(routing_id_));
    DCHECK(rv);
  }

  Destroy();
}

IPC_DEFINE_MESSAGE_MAP(RenderWidgetHost)
  IPC_MESSAGE_HANDLER(ViewHostMsg_RenderViewReady, OnMsgRenderViewReady)
  IPC_MESSAGE_HANDLER(ViewHostMsg_RenderViewGone, OnMsgRenderViewGone)
  IPC_MESSAGE_HANDLER(ViewHostMsg_Close, OnMsgClose)
  IPC_MESSAGE_HANDLER(ViewHostMsg_RequestMove, OnMsgRequestMove)
  IPC_MESSAGE_HANDLER(ViewHostMsg_PaintRect, OnMsgPaintRect)
  IPC_MESSAGE_HANDLER(ViewHostMsg_ScrollRect, OnMsgScrollRect)
  IPC_MESSAGE_HANDLER(ViewHostMsg_HandleInputEvent_ACK, OnMsgInputEventAck)
  IPC_MESSAGE_HANDLER(ViewHostMsg_Focus, OnMsgFocus)
  IPC_MESSAGE_HANDLER(ViewHostMsg_Blur, OnMsgBlur)
  IPC_MESSAGE_HANDLER(ViewHostMsg_SetCursor, OnMsgSetCursor)
  IPC_MESSAGE_HANDLER(ViewHostMsg_ImeUpdateStatus, OnMsgImeUpdateStatus)
  IPC_MESSAGE_HANDLER_GENERIC(ViewHostMsg_ShowPopup, OnMsgShowPopup(msg))
#if defined(OS_LINUX)
  IPC_MESSAGE_HANDLER(ViewHostMsg_CreatePluginContainer,
                      OnMsgCreatePluginContainer)
  IPC_MESSAGE_HANDLER(ViewHostMsg_DestroyPluginContainer,
                      OnMsgDestroyPluginContainer)
#endif
  IPC_MESSAGE_UNHANDLED_ERROR()
IPC_END_MESSAGE_MAP()

bool RenderWidgetHost::Send(IPC::Message* msg) {
  return process_->Send(msg);
}

void RenderWidgetHost::WasHidden() {
  is_hidden_ = true;

  // Don't bother reporting hung state when we aren't the active tab.
  StopHangMonitorTimeout();

  // If we have a renderer, then inform it that we are being hidden so it can
  // reduce its resource utilization.
  Send(new ViewMsg_WasHidden(routing_id_));

  // TODO(darin): what about constrained windows?  it doesn't look like they
  // see a message when their parent is hidden.  maybe there is something more
  // generic we can do at the TabContents API level instead of relying on
  // Windows messages.

  // Tell the RenderProcessHost we were hidden.
  process_->WidgetHidden();

  bool is_visible = false;
  NotificationService::current()->Notify(
      NotificationType::RENDER_WIDGET_VISIBILITY_CHANGED,
      Source<RenderWidgetHost>(this),
      Details<bool>(&is_visible));
}

void RenderWidgetHost::WasRestored() {
  // When we create the widget, it is created as *not* hidden.
  if (!is_hidden_)
    return;
  is_hidden_ = false;

  BackingStore* backing_store = BackingStoreManager::Lookup(this);
  // If we already have a backing store for this widget, then we don't need to
  // repaint on restore _unless_ we know that our backing store is invalid.
  bool needs_repainting;
  if (needs_repainting_on_restore_ || !backing_store) {
    needs_repainting = true;
    needs_repainting_on_restore_ = false;
  } else {
    needs_repainting = false;
  }
  Send(new ViewMsg_WasRestored(routing_id_, needs_repainting));

  process_->WidgetRestored();

  bool is_visible = true;
  NotificationService::current()->Notify(
      NotificationType::RENDER_WIDGET_VISIBILITY_CHANGED,
      Source<RenderWidgetHost>(this),
      Details<bool>(&is_visible));
}

void RenderWidgetHost::WasResized() {
  if (resize_ack_pending_ || !process_->HasConnection() || !view_ ||
      !renderer_initialized_) {
    return;
  }

  gfx::Rect view_bounds = view_->GetViewBounds();
  gfx::Size new_size(view_bounds.width(), view_bounds.height());

  // Avoid asking the RenderWidget to resize to its current size, since it
  // won't send us a PaintRect message in that case.
  if (new_size == current_size_)
    return;

  // We don't expect to receive an ACK when the requested size is empty.
  if (!new_size.IsEmpty())
    resize_ack_pending_ = true;

  if (!Send(new ViewMsg_Resize(routing_id_, new_size,
                               GetRootWindowResizerRect())))
    resize_ack_pending_ = false;
}

void RenderWidgetHost::GotFocus() {
  Focus();
}

void RenderWidgetHost::Focus() {
  Send(new ViewMsg_SetFocus(routing_id_, true));
}

void RenderWidgetHost::Blur() {
  Send(new ViewMsg_SetFocus(routing_id_, false));
}

void RenderWidgetHost::LostCapture() {
  Send(new ViewMsg_MouseCaptureLost(routing_id_));
}

void RenderWidgetHost::ViewDestroyed() {
  // TODO(evanm): tracking this may no longer be necessary;
  // eliminate this function if so.
  view_ = NULL;
}

void RenderWidgetHost::SetIsLoading(bool is_loading) {
  is_loading_ = is_loading;
  if (!view_)
    return;
  view_->SetIsLoading(is_loading);
}

BackingStore* RenderWidgetHost::GetBackingStore(bool force_create) {
  // We should not be asked to paint while we are hidden.  If we are hidden,
  // then it means that our consumer failed to call WasRestored. If we're not
  // force creating the backing store, it's OK since we can feel free to give
  // out our cached one if we have it.
  DCHECK(!is_hidden_ || !force_create) <<
      "GetBackingStore called while hidden!";

  // We should never be called recursively; this can theoretically lead to
  // infinite recursion and almost certainly leads to lower performance.
  DCHECK(!in_get_backing_store_) << "GetBackingStore called recursively!";
  in_get_backing_store_ = true;

  // We might have a cached backing store that we can reuse!
  BackingStore* backing_store =
      BackingStoreManager::GetBackingStore(this, current_size_);
  if (!force_create) {
    in_get_backing_store_ = false;
    return backing_store;
  }

  // If we fail to find a backing store in the cache, send out a request
  // to the renderer to paint the view if required.
  if (!backing_store && !repaint_ack_pending_ && !resize_ack_pending_ &&
      !view_being_painted_) {
    repaint_start_time_ = TimeTicks::Now();
    repaint_ack_pending_ = true;
    Send(new ViewMsg_Repaint(routing_id_, current_size_));
  }

  // When we have asked the RenderWidget to resize, and we are still waiting on
  // a response, block for a little while to see if we can't get a response
  // before returning the old (incorrectly sized) backing store.
  if (resize_ack_pending_ || !backing_store) {
    IPC::Message msg;
    TimeDelta max_delay = TimeDelta::FromMilliseconds(kPaintMsgTimeoutMS);
    if (process_->WaitForPaintMsg(routing_id_, max_delay, &msg)) {
      ViewHostMsg_PaintRect::Dispatch(
          &msg, this, &RenderWidgetHost::OnMsgPaintRect);
      backing_store = BackingStoreManager::GetBackingStore(this, current_size_);
    }
  }

  in_get_backing_store_ = false;
  return backing_store;
}

BackingStore* RenderWidgetHost::AllocBackingStore(const gfx::Size& size) {
  if (!view_)
    return NULL;
  return view_->AllocBackingStore(size);
}

void RenderWidgetHost::StartHangMonitorTimeout(TimeDelta delay) {
  time_when_considered_hung_ = Time::Now() + delay;

  // If we already have a timer that will expire at or before the given delay,
  // then we have nothing more to do now.
  if (hung_renderer_timer_.IsRunning() &&
      hung_renderer_timer_.GetCurrentDelay() <= delay)
    return;

  // Either the timer is not yet running, or we need to adjust the timer to
  // fire sooner.
  hung_renderer_timer_.Stop();
  hung_renderer_timer_.Start(delay, this,
      &RenderWidgetHost::CheckRendererIsUnresponsive);
}

void RenderWidgetHost::RestartHangMonitorTimeout() {
  StartHangMonitorTimeout(TimeDelta::FromMilliseconds(kHungRendererDelayMs));
}

void RenderWidgetHost::StopHangMonitorTimeout() {
  time_when_considered_hung_ = Time();
  RendererIsResponsive();

  // We do not bother to stop the hung_renderer_timer_ here in case it will be
  // started again shortly, which happens to be the common use case.
}

void RenderWidgetHost::SystemThemeChanged() {
  Send(new ViewMsg_ThemeChanged(routing_id_));
}

void RenderWidgetHost::ForwardMouseEvent(const WebMouseEvent& mouse_event) {
  // Avoid spamming the renderer with mouse move events.  It is important
  // to note that WM_MOUSEMOVE events are anyways synthetic, but since our
  // thread is able to rapidly consume WM_MOUSEMOVE events, we may get way
  // more WM_MOUSEMOVE events than we wish to send to the renderer.
  if (mouse_event.type == WebInputEvent::MouseMove) {
    if (mouse_move_pending_) {
      next_mouse_move_.reset(new WebMouseEvent(mouse_event));
      return;
    }
    mouse_move_pending_ = true;
  } else if (mouse_event.type == WebInputEvent::MouseDown) {
    OnUserGesture();
  }

  ForwardInputEvent(mouse_event, sizeof(WebMouseEvent));
}

void RenderWidgetHost::ForwardWheelEvent(
    const WebMouseWheelEvent& wheel_event) {
  ForwardInputEvent(wheel_event, sizeof(WebMouseWheelEvent));
}

void RenderWidgetHost::ForwardKeyboardEvent(
    const NativeWebKeyboardEvent& key_event) {
  if (key_event.type == WebKeyboardEvent::Char &&
      (key_event.windowsKeyCode == base::VKEY_RETURN ||
       key_event.windowsKeyCode == base::VKEY_SPACE)) {
    OnUserGesture();
  }

  // Double check the type to make sure caller hasn't sent us nonsense that
  // will mess up our key queue.
  if (WebInputEvent::isKeyboardEventType(key_event.type)) {
    // Don't add this key to the queue if we have no way to send the message...
    if (!process_->HasConnection())
      return;

    // Put all WebKeyboardEvent objects in a queue since we can't trust the
    // renderer and we need to give something to the UnhandledInputEvent
    // handler.
    key_queue_.push(key_event);
    HISTOGRAM_COUNTS_100("Renderer.KeyboardQueueSize", key_queue_.size());
  }

  // Only forward the non-native portions of our event.
  ForwardInputEvent(key_event, sizeof(WebKeyboardEvent));
}

void RenderWidgetHost::ForwardInputEvent(const WebInputEvent& input_event,
                                         int event_size) {
  if (!process_->HasConnection())
    return;

  IPC::Message* message = new ViewMsg_HandleInputEvent(routing_id_);
  message->WriteData(
      reinterpret_cast<const char*>(&input_event), event_size);
  input_event_start_time_ = TimeTicks::Now();
  Send(message);

  // Any input event cancels a pending mouse move event.
  next_mouse_move_.reset();

  StartHangMonitorTimeout(TimeDelta::FromMilliseconds(kHungRendererDelayMs));
}

void RenderWidgetHost::ForwardEditCommand(const std::string& name,
      const std::string& value) {
  // We don't need an implementation of this function here since the
  // only place we use this is for the case of dropdown menus and other
  // edge cases for which edit commands don't make sense.
}

void RenderWidgetHost::RendererExited() {
  // Clearing this flag causes us to re-create the renderer when recovering
  // from a crashed renderer.
  renderer_initialized_ = false;

  // Must reset these to ensure that mouse move events work with a new renderer.
  mouse_move_pending_ = false;
  next_mouse_move_.reset();

  // Reset some fields in preparation for recovering from a crash.
  resize_ack_pending_ = false;
  current_size_ = gfx::Size();
  is_hidden_ = false;

  if (view_) {
    view_->RenderViewGone();
    view_ = NULL;  // The View should be deleted by RenderViewGone.
  }

  BackingStoreManager::RemoveBackingStore(this);
}

void RenderWidgetHost::UpdateTextDirection(WebTextDirection direction) {
  text_direction_updated_ = true;
  text_direction_ = direction;
}

void RenderWidgetHost::CancelUpdateTextDirection() {
  if (text_direction_updated_)
    text_direction_canceled_ = true;
}

void RenderWidgetHost::NotifyTextDirection() {
  if (text_direction_updated_) {
    if (!text_direction_canceled_)
      Send(new ViewMsg_SetTextDirection(routing_id(),
                                        static_cast<int>(text_direction_)));
    text_direction_updated_ = false;
    text_direction_canceled_ = false;
  }
}

void RenderWidgetHost::ImeSetInputMode(bool activate) {
  Send(new ViewMsg_ImeSetInputMode(routing_id(), activate));
}

void RenderWidgetHost::ImeSetComposition(const std::wstring& ime_string,
                                         int cursor_position,
                                         int target_start,
                                         int target_end) {
  Send(new ViewMsg_ImeSetComposition(routing_id(), 0, cursor_position,
                                     target_start, target_end, ime_string));
}

void RenderWidgetHost::ImeConfirmComposition(const std::wstring& ime_string) {
  Send(new ViewMsg_ImeSetComposition(routing_id(), 1, -1, -1, -1, ime_string));
}

void RenderWidgetHost::ImeCancelComposition() {
  std::wstring empty_string;
  Send(new ViewMsg_ImeSetComposition(routing_id(), -1, -1, -1, -1,
                                     empty_string));
}

gfx::Rect RenderWidgetHost::GetRootWindowResizerRect() const {
  return gfx::Rect();
}

void RenderWidgetHost::Destroy() {
  NotificationService::current()->Notify(
      NotificationType::RENDER_WIDGET_HOST_DESTROYED,
      Source<RenderWidgetHost>(this),
      NotificationService::NoDetails());

  // Tell the view to die.
  // Note that in the process of the view shutting down, it can call a ton
  // of other messages on us.  So if you do any other deinitialization here,
  // do it after this call to view_->Destroy().
  if (view_)
    view_->Destroy();

  delete this;
}

void RenderWidgetHost::CheckRendererIsUnresponsive() {
  // If we received a call to StopHangMonitorTimeout.
  if (time_when_considered_hung_.is_null())
    return;

  // If we have not waited long enough, then wait some more.
  Time now = Time::Now();
  if (now < time_when_considered_hung_) {
    StartHangMonitorTimeout(time_when_considered_hung_ - now);
    return;
  }

  // OK, looks like we have a hung renderer!
  NotificationService::current()->Notify(
      NotificationType::RENDERER_PROCESS_HANG,
      Source<RenderWidgetHost>(this),
      NotificationService::NoDetails());
  is_unresponsive_ = true;
  NotifyRendererUnresponsive();
}

void RenderWidgetHost::RendererIsResponsive() {
  if (is_unresponsive_) {
    is_unresponsive_ = false;
    NotifyRendererResponsive();
  }
}

void RenderWidgetHost::OnMsgRenderViewReady() {
  WasResized();
}

void RenderWidgetHost::OnMsgRenderViewGone() {
  // TODO(evanm): This synchronously ends up calling "delete this".
  // Is that really what we want in response to this message?  I'm matching
  // previous behavior of the code here.
  Destroy();
}

void RenderWidgetHost::OnMsgClose() {
  Shutdown();
}

void RenderWidgetHost::OnMsgRequestMove(const gfx::Rect& pos) {
  // Note that we ignore the position.
  if (view_) {
    view_->SetSize(pos.size());
    Send(new ViewMsg_Move_ACK(routing_id_));
  }
}

void RenderWidgetHost::OnMsgPaintRect(
    const ViewHostMsg_PaintRect_Params& params) {
  TimeTicks paint_start = TimeTicks::Now();

  // Update our knowledge of the RenderWidget's size.
  current_size_ = params.view_size;

  bool is_resize_ack =
      ViewHostMsg_PaintRect_Flags::is_resize_ack(params.flags);

  // resize_ack_pending_ needs to be cleared before we call DidPaintRect, since
  // that will end up reaching GetBackingStore.
  if (is_resize_ack) {
    DCHECK(resize_ack_pending_);
    resize_ack_pending_ = false;
  }

  bool is_repaint_ack =
      ViewHostMsg_PaintRect_Flags::is_repaint_ack(params.flags);
  if (is_repaint_ack) {
    repaint_ack_pending_ = false;
    TimeDelta delta = TimeTicks::Now() - repaint_start_time_;
    UMA_HISTOGRAM_TIMES("MPArch.RWH_RepaintDelta", delta);
  }

  DCHECK(!params.bitmap_rect.IsEmpty());
  DCHECK(!params.view_size.IsEmpty());

  const size_t size = params.bitmap_rect.height() *
                      params.bitmap_rect.width() * 4;
  TransportDIB* dib = process_->GetTransportDIB(params.bitmap);
  if (dib) {
    if (dib->size() < size) {
      DLOG(WARNING) << "Transport DIB too small for given rectangle";
      process()->ReceivedBadMessage(ViewHostMsg_PaintRect__ID);
    } else {
      // Paint the backing store. This will update it with the renderer-supplied
      // bits. The view will read out of the backing store later to actually
      // draw to the screen.
      PaintBackingStoreRect(dib, params.bitmap_rect, params.view_size);
    }
  }

  // ACK early so we can prefetch the next PaintRect if there is a next one.
  // This must be done AFTER we're done painting with the bitmap supplied by the
  // renderer. This ACK is a signal to the renderer that the backing store can
  // be re-used, so the bitmap may be invalid after this call.
  Send(new ViewMsg_PaintRect_ACK(routing_id_));

  // We don't need to update the view if the view is hidden. We must do this
  // early return after the ACK is sent, however, or the renderer will not send
  // is more data.
  if (is_hidden_)
    return;

  // Now paint the view. Watch out: it might be destroyed already.
  if (view_) {
    view_->MovePluginWindows(params.plugin_window_moves);
    view_being_painted_ = true;
    view_->DidPaintRect(params.bitmap_rect);
    view_being_painted_ = false;
  }

  if (paint_observer_.get())
    paint_observer_->RenderWidgetHostDidPaint(this);

  // If we got a resize ack, then perhaps we have another resize to send?
  if (is_resize_ack && view_) {
    gfx::Rect view_bounds = view_->GetViewBounds();
    if (current_size_.width() != view_bounds.width() ||
        current_size_.height() != view_bounds.height()) {
      WasResized();
    }
  }

  if (painting_observer_)
    painting_observer_->WidgetDidUpdateBackingStore(this);

  // Log the time delta for processing a paint message.
  TimeDelta delta = TimeTicks::Now() - paint_start;
  UMA_HISTOGRAM_TIMES("MPArch.RWH_OnMsgPaintRect", delta);
}

void RenderWidgetHost::OnMsgScrollRect(
    const ViewHostMsg_ScrollRect_Params& params) {
  TimeTicks scroll_start = TimeTicks::Now();

  DCHECK(!params.view_size.IsEmpty());

  const size_t size = params.bitmap_rect.height() *
                      params.bitmap_rect.width() * 4;
  TransportDIB* dib = process_->GetTransportDIB(params.bitmap);
  if (dib) {
    if (dib->size() < size) {
      LOG(WARNING) << "Transport DIB too small for given rectangle";
      process()->ReceivedBadMessage(ViewHostMsg_PaintRect__ID);
    } else {
      // Scroll the backing store.
      ScrollBackingStoreRect(dib, params.bitmap_rect,
                             params.dx, params.dy,
                             params.clip_rect, params.view_size);
    }
  }

  // ACK early so we can prefetch the next ScrollRect if there is a next one.
  // This must be done AFTER we're done painting with the bitmap supplied by the
  // renderer. This ACK is a signal to the renderer that the backing store can
  // be re-used, so the bitmap may be invalid after this call.
  Send(new ViewMsg_ScrollRect_ACK(routing_id_));

  // We don't need to update the view if the view is hidden. We must do this
  // early return after the ACK is sent, however, or the renderer will not send
  // is more data.
  if (is_hidden_)
    return;

  // Paint the view. Watch out: it might be destroyed already.
  if (view_) {
    view_being_painted_ = true;
    view_->MovePluginWindows(params.plugin_window_moves);
    view_->DidScrollRect(params.clip_rect, params.dx, params.dy);
    view_being_painted_ = false;
  }

  if (painting_observer_)
    painting_observer_->WidgetDidUpdateBackingStore(this);

  // Log the time delta for processing a scroll message.
  TimeDelta delta = TimeTicks::Now() - scroll_start;
  UMA_HISTOGRAM_TIMES("MPArch.RWH_OnMsgScrollRect", delta);
}

void RenderWidgetHost::OnMsgInputEventAck(const IPC::Message& message) {
  // Log the time delta for processing an input event.
  TimeDelta delta = TimeTicks::Now() - input_event_start_time_;
  UMA_HISTOGRAM_TIMES("MPArch.RWH_InputEventDelta", delta);

  // Cancel pending hung renderer checks since the renderer is responsive.
  StopHangMonitorTimeout();

  void* iter = NULL;
  int type = 0;
  bool r = message.ReadInt(&iter, &type);
  DCHECK(r);

  if (type == WebInputEvent::MouseMove) {
    mouse_move_pending_ = false;

    // now, we can send the next mouse move event
    if (next_mouse_move_.get()) {
      DCHECK(next_mouse_move_->type == WebInputEvent::MouseMove);
      ForwardMouseEvent(*next_mouse_move_);
    }
  }

  if (WebInputEvent::isKeyboardEventType(type)) {
    if (key_queue_.size() == 0) {
      LOG(ERROR) << "Got a KeyEvent back from the renderer but we "
                 << "don't seem to have sent it to the renderer!";
    } else if (key_queue_.front().type != type) {
      LOG(ERROR) << "We seem to have a different key type sent from "
                 << "the renderer. (" << key_queue_.front().type << " vs. "
                 << type << "). Ignoring event.";
    } else {
      bool processed = false;
      r = message.ReadBool(&iter, &processed);
      DCHECK(r);

      KeyQueue::value_type front_item = key_queue_.front();
      key_queue_.pop();

      if (!processed) {
        UnhandledKeyboardEvent(front_item);

        // WARNING: This RenderWidgetHost can be deallocated at this point
        // (i.e.  in the case of Ctrl+W, where the call to
        // UnhandledKeyboardEvent destroys this RenderWidgetHost).
      }
    }
  }
}

void RenderWidgetHost::OnMsgFocus() {
  // Only the user can focus a RenderWidgetHost.
  NOTREACHED();
}

void RenderWidgetHost::OnMsgBlur() {
  if (view_) {
    view_->Blur();
  }
}

void RenderWidgetHost::OnMsgSetCursor(const WebCursor& cursor) {
  if (!view_) {
    return;
  }
  view_->UpdateCursor(cursor);
}

void RenderWidgetHost::OnMsgImeUpdateStatus(int control,
                                            const gfx::Rect& caret_rect) {
  if (view_) {
    view_->IMEUpdateStatus(control, caret_rect);
  }
}

void RenderWidgetHost::OnMsgShowPopup(const IPC::Message& message) {
#if defined(OS_MACOSX)
  void* iter = NULL;
  ViewHostMsg_ShowPopup_Params validated_params;
  if (!IPC::ParamTraits<ViewHostMsg_ShowPopup_Params>::Read(&message, &iter,
                                                            &validated_params))
    return;

  view_->ShowPopupWithItems(validated_params.bounds,
                            validated_params.item_height,
                            validated_params.selected_item,
                            validated_params.popup_items);
#else  // OS_WIN || OS_LINUX
  NOTREACHED();
#endif
}

#if defined(OS_LINUX)
void RenderWidgetHost::OnMsgCreatePluginContainer(
    gfx::PluginWindowHandle *container) {
  *container = view_->CreatePluginContainer();
}

void RenderWidgetHost::OnMsgDestroyPluginContainer(
    gfx::PluginWindowHandle container) {
  view_->DestroyPluginContainer(container);
}
#endif

void RenderWidgetHost::PaintBackingStoreRect(TransportDIB* bitmap,
                                             const gfx::Rect& bitmap_rect,
                                             const gfx::Size& view_size) {
  // The view may be destroyed already.
  if (!view_)
    return;

  if (is_hidden_) {
    // Don't bother updating the backing store when we're hidden. Just mark it
    // as being totally invalid. This will cause a complete repaint when the
    // view is restored.
    needs_repainting_on_restore_ = true;
    return;
  }

  bool needs_full_paint = false;
  BackingStore* backing_store =
      BackingStoreManager::PrepareBackingStore(this, view_size,
                                               process_->process().handle(),
                                               bitmap, bitmap_rect,
                                               &needs_full_paint);
  DCHECK(backing_store != NULL);
  if (needs_full_paint) {
    repaint_start_time_ = TimeTicks::Now();
    repaint_ack_pending_ = true;
    Send(new ViewMsg_Repaint(routing_id_, view_size));
  }
}

void RenderWidgetHost::ScrollBackingStoreRect(TransportDIB* bitmap,
                                              const gfx::Rect& bitmap_rect,
                                              int dx, int dy,
                                              const gfx::Rect& clip_rect,
                                              const gfx::Size& view_size) {
  if (is_hidden_) {
    // Don't bother updating the backing store when we're hidden. Just mark it
    // as being totally invalid. This will cause a complete repaint when the
    // view is restored.
    needs_repainting_on_restore_ = true;
    return;
  }

  // TODO(darin): do we need to do something else if our backing store is not
  // the same size as the advertised view?  maybe we just assume there is a
  // full paint on its way?
  BackingStore* backing_store = BackingStoreManager::Lookup(this);
  if (!backing_store || (backing_store->size() != view_size))
    return;
  backing_store->ScrollRect(process_->process().handle(), bitmap, bitmap_rect,
                            dx, dy, clip_rect, view_size);
}
