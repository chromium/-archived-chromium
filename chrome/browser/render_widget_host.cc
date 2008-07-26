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

#include "chrome/browser/render_widget_host.h"

#include <atlbase.h>
#include <atlapp.h>

#include "base/gfx/bitmap_header.h"
#include "base/histogram.h"
#include "chrome/app/chrome_dll_resource.h"
#include "chrome/browser/render_process_host.h"
#include "chrome/browser/render_widget_helper.h"
#include "chrome/browser/render_widget_host_view.h"
#include "chrome/common/mru_cache.h"
#include "chrome/common/win_util.h"
#include "chrome/views/view.h"
#include "webkit/glue/webcursor.h"
#include "webkit/glue/webinputevent.h"

// How long to (synchronously) wait for the renderer to respond with a
// PaintRect message, when our backing-store is invalid, before giving up and
// returning a null or incorrectly sized backing-store from GetBackingStore.
// This timeout impacts the "choppiness" of our window resize perf.
static const int kPaintMsgTimeoutMS = 40;

// Give the renderer at most 10 seconds to respond before we treat it as hung.
static const int kHungRendererDelayMs = 10000;

///////////////////////////////////////////////////////////////////////////////
// RenderWidget::BackingStore

RenderWidgetHost::BackingStore::BackingStore(const gfx::Size& size)
    : size_(size) {
  HDC screen_dc = ::GetDC(NULL);
  hdc_ = CreateCompatibleDC(screen_dc);
  int color_depth = GetDeviceCaps(screen_dc, BITSPIXEL);

  // Color depths less than 16 bpp require a palette to be specified in the
  // BITMAPINFO structure passed to CreateDIBSection. Instead of creating
  // the palette, we specify the desired color depth as 16 which allows the
  // OS to come up with an approximation. Tested this with 8bpp.
  if (color_depth < 16)
    color_depth = 16;

  BITMAPINFOHEADER hdr = {0};
  gfx::CreateBitmapHeaderWithColorDepth(size.width(), size.height(),
                                        color_depth, &hdr);
  void* data = NULL;
  HANDLE bitmap = CreateDIBSection(hdc_, reinterpret_cast<BITMAPINFO*>(&hdr),
                                   0, &data, NULL, 0);
  SelectObject(hdc_, bitmap);
  ::ReleaseDC(NULL, screen_dc);
}

RenderWidgetHost::BackingStore::~BackingStore() {
  DCHECK(hdc_);

  HBITMAP bitmap =
      reinterpret_cast<HBITMAP>(GetCurrentObject(hdc_, OBJ_BITMAP));
  DeleteDC(hdc_);
  DeleteObject(bitmap);
}

///////////////////////////////////////////////////////////////////////////////
// RenderWidget::BackingStoreCache

class RenderWidgetHost::BackingStoreCache {
 public:
  // The number of backing stores to cache.
  enum { kMaxSize = 5 };

  static void Add(RenderWidgetHost* host, BackingStore* backing_store) {
    // TODO(darin): Re-enable once we have a fix for bug 1143208.
    if (!cache_)
      cache_ = new Cache(kMaxSize);
    cache_->Put(host, backing_store);
  }

  static BackingStore* Remove(RenderWidgetHost* host) {
    // TODO(darin): Re-enable once we have a fix for bug 1143208.
    if (!cache_)
      return NULL;

    Cache::iterator it = cache_->Peek(host);
    if (it == cache_->end())
      return NULL;

    BackingStore* result = it->second;

    // Remove from the cache without deleting the backing store object.
    it->second = NULL;
    cache_->Erase(it);

    if (cache_->size() == 0) {
      delete cache_;
      cache_ = NULL;
    }

    return result;
  }

 private:
  ~BackingStoreCache();  // not intended to be instantiated

  typedef OwningMRUCache<RenderWidgetHost*, BackingStore*> Cache;
  static Cache* cache_;
};

// static
RenderWidgetHost::BackingStoreCache::Cache*
    RenderWidgetHost::BackingStoreCache::cache_ = NULL;

///////////////////////////////////////////////////////////////////////////////
// RenderWidgetHost

RenderWidgetHost::RenderWidgetHost(RenderProcessHost* process, int routing_id)
    : process_(process),
      routing_id_(routing_id),
      resize_ack_pending_(false),
      mouse_move_pending_(false),
      view_(NULL),
      is_loading_(false),
      is_hidden_(false),
      suppress_view_updating_(false),
      needs_repainting_on_restore_(false),
      hung_renderer_factory_(this),
      is_unresponsive_(false) {
  if (routing_id_ == MSG_ROUTING_NONE)
    routing_id_ = process_->widget_helper()->GetNextRoutingID();

  process_->Attach(this, routing_id_);
  // Because the widget initializes as is_hidden_ == false,
  // tell the process host that we're alive.
  process_->WidgetRestored();
}

RenderWidgetHost::~RenderWidgetHost() {
  // Clear our current or cached backing store if either remains.
  backing_store_.reset(BackingStoreCache::Remove(this));

  process_->Release(routing_id_);
}

void RenderWidgetHost::Init() {
  DCHECK(process_->channel());

  // Send the ack along with the information on placement.
  HWND plugin_hwnd = view_->GetPluginHWND();
  Send(new ViewMsg_CreatingNew_ACK(routing_id_, plugin_hwnd));

  WasResized();
}

///////////////////////////////////////////////////////////////////////////////
// RenderWidgetHost, protected:

IPC_DEFINE_MESSAGE_MAP(RenderWidgetHost)
  IPC_MESSAGE_HANDLER(ViewHostMsg_RendererReady, OnMsgRendererReady)
  IPC_MESSAGE_HANDLER(ViewHostMsg_RendererGone, OnMsgRendererGone)
  IPC_MESSAGE_HANDLER(ViewHostMsg_Close, OnMsgClose)
  IPC_MESSAGE_HANDLER(ViewHostMsg_RequestMove, OnMsgRequestMove)
  IPC_MESSAGE_HANDLER(ViewHostMsg_PaintRect, OnMsgPaintRect)
  IPC_MESSAGE_HANDLER(ViewHostMsg_ScrollRect, OnMsgScrollRect)
  IPC_MESSAGE_HANDLER(ViewHostMsg_HandleInputEvent_ACK, OnMsgInputEventAck)
  IPC_MESSAGE_HANDLER(ViewHostMsg_Focus, OnMsgFocus)
  IPC_MESSAGE_HANDLER(ViewHostMsg_Blur, OnMsgBlur)
  IPC_MESSAGE_HANDLER(ViewHostMsg_SetCursor, OnMsgSetCursor)
  IPC_MESSAGE_HANDLER(ViewHostMsg_ImeUpdateStatus, OnMsgImeUpdateStatus)
  IPC_MESSAGE_UNHANDLED_ERROR()
IPC_END_MESSAGE_MAP()

void RenderWidgetHost::OnMsgRendererReady() {
  WasResized();
}

void RenderWidgetHost::OnMsgRendererGone() {
  // TODO(evanm): This synchronously ends up calling "delete this".
  // Is that really what we want in response to this message?  I'm matching
  // previous behavior of the code here.
  Destroy();
}

void RenderWidgetHost::OnMsgClose() {
  Shutdown();
}

void RenderWidgetHost::OnMsgRequestMove(const gfx::Rect& pos) {
  // Don't allow renderer widgets to move themselves by default.  Maybe this
  // policy will change if we add more types of widgets.
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

  DCHECK(params.bitmap);
  DCHECK(!params.bitmap_rect.IsEmpty());
  DCHECK(!params.view_size.IsEmpty());

  PaintRect(params.bitmap, params.bitmap_rect, params.view_size);

  // ACK early so we can prefetch the next PaintRect if there is a next one.
  Send(new ViewMsg_PaintRect_ACK(routing_id_));

  // TODO(darin): This should really be done by the view_!
  MovePluginWindows(params.plugin_window_moves);

  // The view might be destroyed already.  Check for this case.
  if (view_ && !suppress_view_updating_)
    view_->DidPaintRect(params.bitmap_rect);

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

  // Log the time delta for processing a paint message.
  TimeDelta delta = TimeTicks::Now() - paint_start;
  UMA_HISTOGRAM_TIMES(L"MPArch.RWH_OnMsgPaintRect", delta);
}

void RenderWidgetHost::OnMsgScrollRect(
    const ViewHostMsg_ScrollRect_Params& params) {
  TimeTicks scroll_start = TimeTicks::Now();

  DCHECK(!params.view_size.IsEmpty());

  ScrollRect(params.bitmap, params.bitmap_rect, params.dx, params.dy,
             params.clip_rect, params.view_size);

  // ACK early so we can prefetch the next ScrollRect if there is a next one.
  Send(new ViewMsg_ScrollRect_ACK(routing_id_));

  // TODO(darin): This should really be done by the view_!
  MovePluginWindows(params.plugin_window_moves);

  // The view might be destroyed already. Check for this case
  if (view_)
    view_->DidScrollRect(params.clip_rect, params.dx, params.dy);

  // Log the time delta for processing a scroll message.
  TimeDelta delta = TimeTicks::Now() - scroll_start;
  UMA_HISTOGRAM_TIMES(L"MPArch.RWH_OnMsgScrollRect", delta);
}

void RenderWidgetHost::MovePluginWindows(
    const std::vector<WebPluginGeometry>& plugin_window_moves) {
  HDWP defer_window_pos_info =
      ::BeginDeferWindowPos(static_cast<int>(plugin_window_moves.size()));

  if (!defer_window_pos_info) {
    NOTREACHED();
    return;
  }

  for (size_t i = 0; i < plugin_window_moves.size(); ++i) {
    unsigned long flags = 0;
    const WebPluginGeometry& move = plugin_window_moves[i];

    if (move.visible)
      flags |= SWP_SHOWWINDOW;
    else
      flags |= SWP_HIDEWINDOW;

    HRGN hrgn = ::CreateRectRgn(move.clip_rect.x(),
                                move.clip_rect.y(),
                                move.clip_rect.right(),
                                move.clip_rect.bottom());

    // Note: System will own the hrgn after we call SetWindowRgn,
    // so we don't need to call DeleteObject(hrgn)
    ::SetWindowRgn(move.window, hrgn, !move.clip_rect.IsEmpty());

    defer_window_pos_info = ::DeferWindowPos(defer_window_pos_info,
                                             move.window, NULL,
                                             move.window_rect.x(),
                                             move.window_rect.y(),
                                             move.window_rect.width(),
                                             move.window_rect.height(), flags);
    if (!defer_window_pos_info) {
      return;
    }
  }

  ::EndDeferWindowPos(defer_window_pos_info);
}

void RenderWidgetHost::OnMsgInputEventAck(const IPC::Message& message) {
  // Log the time delta for processing an input event.
  TimeDelta delta = TimeTicks::Now() - input_event_start_time_;
  UMA_HISTOGRAM_TIMES(L"MPArch.RWH_InputEventDelta", delta);

  // Cancel pending hung renderer checks since the renderer is responsive.
  ResetHangMonitorTimeout();
  RendererIsResponsive();

  void* iter = NULL;
  int type = 0;
  bool r = message.ReadInt(&iter, &type);
  DCHECK(r);

  if (type == WebInputEvent::MOUSE_MOVE) {
    mouse_move_pending_ = false;

    // now, we can send the next mouse move event
    if (next_mouse_move_.get()) {
      DCHECK(next_mouse_move_->type == WebInputEvent::MOUSE_MOVE);
      ForwardMouseEvent(*next_mouse_move_);
    }
  }

  const char* data = NULL;
  int length = 0;
  if (message.ReadData(&iter, &data, &length)) {
    const WebInputEvent* input_event =
        reinterpret_cast<const WebInputEvent*>(data);
    UnhandledInputEvent(*input_event);
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

void RenderWidgetHost::OnMsgImeUpdateStatus(ViewHostMsg_ImeControl control,
                                            int x, int y) {
  if (view_) {
    view_->IMEUpdateStatus(control, x, y);
  }
}

///////////////////////////////////////////////////////////////////////////////

void RenderWidgetHost::WasHidden() {
  is_hidden_ = true;

  // Don't bother reporting hung state when we aren't the active tab.
  ResetHangMonitorTimeout();
  RendererIsResponsive();

  // If we have a renderer, then inform it that we are being hidden so it can
  // reduce its resource utilization.
  Send(new ViewMsg_WasHidden(routing_id_));

  // Yield the backing store (allows this memory to be freed).
  if (backing_store_.get())
    BackingStoreCache::Add(this, backing_store_.release());

  // TODO(darin): what about constrained windows?  it doesn't look like they
  // see a message when their parent is hidden.  maybe there is something more
  // generic we can do at the TabContents API level instead of relying on
  // Windows messages.

  // Tell the RenderProcessHost we were hidden.
  process_->WidgetHidden();
}

void RenderWidgetHost::WasRestored() {
  // When we create the widget, it is created as *not* hidden.
  if (!is_hidden_)
    return;
  is_hidden_ = false;

  DCHECK(!backing_store_.get());
  backing_store_.reset(BackingStoreCache::Remove(this));

  // If we already have a backing store for this widget, then we don't need to
  // repaint on restore _unless_ we know that our backing store is invalid.
  bool needs_repainting;
  if (needs_repainting_on_restore_ || !backing_store_.get()) {
    needs_repainting = true;
    needs_repainting_on_restore_ = false;
  } else {
    needs_repainting = false;
  }
  Send(new ViewMsg_WasRestored(routing_id_, needs_repainting));

  process_->WidgetRestored();
}

void RenderWidgetHost::WasResized() {
  if (resize_ack_pending_ || !process_->channel())
    return;

  gfx::Rect view_bounds = view_->GetViewBounds();
  gfx::Size new_size(view_bounds.width(), view_bounds.height());

  // Avoid asking the RenderWidget to resize to its current size, since it
  // won't send us a PaintRect message in that case.
  if (new_size == current_size_)
    return;

  // We don't expect to receive an ACK when the requested size is empty.
  if (!new_size.IsEmpty())
    resize_ack_pending_ = true;

  if (!Send(new ViewMsg_Resize(routing_id_, new_size)))
    resize_ack_pending_ = false;
}

void RenderWidgetHost::ForwardMouseEvent(const WebMouseEvent& mouse_event) {
  // Avoid spamming the renderer with mouse move events.  It is important
  // to note that WM_MOUSEMOVE events are anyways synthetic, but since our
  // thread is able to rapidly consume WM_MOUSEMOVE events, we may get way
  // more WM_MOUSEMOVE events than we wish to send to the renderer.
  if (mouse_event.type == WebInputEvent::MOUSE_MOVE) {
    if (mouse_move_pending_) {
      next_mouse_move_.reset(new WebMouseEvent(mouse_event));
      return;
    }
    mouse_move_pending_ = true;
  }

  ForwardInputEvent(mouse_event, sizeof(WebMouseEvent));
}

void RenderWidgetHost::ForwardKeyboardEvent(
    const WebKeyboardEvent& key_event) {
  ForwardInputEvent(key_event, sizeof(WebKeyboardEvent));
}

void RenderWidgetHost::ForwardWheelEvent(
    const WebMouseWheelEvent& wheel_event) {
  ForwardInputEvent(wheel_event, sizeof(WebMouseWheelEvent));
}

void RenderWidgetHost::ForwardInputEvent(const WebInputEvent& input_event,
                                         int event_size) {
  if (!process_->channel())
    return;

  IPC::Message* message = new ViewMsg_HandleInputEvent(routing_id_);
  message->WriteData(
      reinterpret_cast<const char*>(&input_event), event_size);
  input_event_start_time_ = TimeTicks::Now();
  Send(message);

  // any input event cancels a pending mouse move event
  next_mouse_move_.reset();

  MessageLoop::current()->PostDelayedTask(FROM_HERE,
      hung_renderer_factory_.NewRunnableMethod(
          &RenderWidgetHost::RendererIsUnresponsive), kHungRendererDelayMs);
}

void RenderWidgetHost::Shutdown() {
  if (process_->channel()) {
    // Tell the renderer object to close.
    process_->ReportExpectingClose(routing_id_);
    bool rv = Send(new ViewMsg_Close(routing_id_));
    DCHECK(rv);
  }

  Destroy();
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

void RenderWidgetHost::Destroy() {
  // Tell the view to die.
  // Note that in the process of the view shutting down, it can call a ton
  // of other messages on us.  So if you do any other deinitialization here,
  // do it after this call to view_->Destroy().
  if (view_)
    view_->Destroy();

  delete this;
}

void RenderWidgetHost::RendererIsUnresponsive() {
  NotificationService::current()->Notify(NOTIFY_RENDERER_PROCESS_HANG,
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

bool RenderWidgetHost::Send(IPC::Message* msg) {
  return process_->Send(msg);
}

void RenderWidgetHost::SetIsLoading(bool is_loading) {
  is_loading_ = is_loading;
  if (!view_)
    return;
  view_->SetIsLoading(is_loading);
}

RenderWidgetHost::BackingStore* RenderWidgetHost::GetBackingStore() {
  // We should not be asked to paint while we are hidden.  If we are hidden,
  // then it means that our consumer failed to call WasRestored.
  DCHECK(!is_hidden_) << "GetBackingStore called while hidden!";

  // We might have a cached backing store that we can reuse!
  if (!backing_store_.get())
    backing_store_.reset(BackingStoreCache::Remove(this));

  // When we have asked the RenderWidget to resize, and we are still waiting on
  // a response, block for a little while to see if we can't get a response
  // before returning the old (incorrectly sized) backing store.
  if (resize_ack_pending_ || !backing_store_.get()) {
    IPC::Message msg;
    TimeDelta max_delay = TimeDelta::FromMilliseconds(kPaintMsgTimeoutMS);
    if (process_->widget_helper()->WaitForPaintMsg(routing_id_, max_delay,
                                                   &msg)) {
      suppress_view_updating_ = true;
      ViewHostMsg_PaintRect::Dispatch(
          &msg, this, &RenderWidgetHost::OnMsgPaintRect);
      suppress_view_updating_ = false;
    }
  }

  return backing_store_.get();
}

void RenderWidgetHost::EnsureBackingStore(const gfx::Rect& view_rect) {
  // We might have a cached backing store that we can reuse!
  if (!backing_store_.get())
    backing_store_.reset(BackingStoreCache::Remove(this));

  // If we already have a backing store, then make sure it is the correct size.
  if (backing_store_.get() && backing_store_->size() == view_rect.size())
    return;

  backing_store_.reset(new BackingStore(view_rect.size()));
}

void RenderWidgetHost::PaintBackingStore(HANDLE bitmap,
                                         const gfx::Rect& bitmap_rect) {
  // TODO(darin): protect against integer overflow
  DWORD size = 4 * bitmap_rect.width() * bitmap_rect.height();
  void* data = MapViewOfFile(bitmap, FILE_MAP_READ, 0, 0, size);

  // These values are shared with gfx::PlatformDevice
  BITMAPINFOHEADER hdr;
  gfx::CreateBitmapHeader(bitmap_rect.width(), bitmap_rect.height(), &hdr);

  // Account for a bitmap_rect that exceeds the bounds of our view
  gfx::Rect view_rect(
      0, 0, backing_store_->size().width(), backing_store_->size().height());
  gfx::Rect paint_rect = view_rect.Intersect(bitmap_rect);

  StretchDIBits(backing_store_->dc(),
                paint_rect.x(),
                paint_rect.y(),
                paint_rect.width(),
                paint_rect.height(),
                0, 0,  // source x,y
                paint_rect.width(),
                paint_rect.height(),
                data,
                reinterpret_cast<BITMAPINFO*>(&hdr),
                DIB_RGB_COLORS,
                SRCCOPY);

  UnmapViewOfFile(data);
}

void RenderWidgetHost::PaintRect(HANDLE bitmap, const gfx::Rect& bitmap_rect,
                                 const gfx::Size& view_size) {
  if (is_hidden_) {
    needs_repainting_on_restore_ = true;
    return;
  }

  // The bitmap received is valid only in the renderer process.
  HANDLE valid_bitmap =
      win_util::GetSectionFromProcess(bitmap, process_->process(), true);
  if (valid_bitmap) {
    // We use the view size according to the render view, which may not be
    // quite the same as the size of our window.
    gfx::Rect view_rect(0, 0, view_size.width(), view_size.height());

    EnsureBackingStore(view_rect);

    PaintBackingStore(valid_bitmap, bitmap_rect);
    CloseHandle(valid_bitmap);
  }
}

void RenderWidgetHost::ScrollRect(HANDLE bitmap, const gfx::Rect& bitmap_rect,
                                  int dx, int dy, const gfx::Rect& clip_rect,
                                  const gfx::Size& view_size) {
  if (is_hidden_) {
    needs_repainting_on_restore_ = true;
    return;
  }

  // TODO(darin): do we need to do something else if our backing store is not
  // the same size as the advertised view?  maybe we just assume there is a
  // full paint on its way?
  if (backing_store_->size() != view_size)
    return;

  RECT damaged_rect, r = clip_rect.ToRECT();
  ScrollDC(backing_store_->dc(), dx, dy, NULL, &r, NULL, &damaged_rect);

  // TODO(darin): this doesn't work if dx and dy are both non-zero!
  DCHECK(dx == 0 || dy == 0);

  // We expect that damaged_rect should equal bitmap_rect.
  DCHECK(gfx::Rect(damaged_rect) == bitmap_rect);

  // The bitmap handle is valid only in the renderer process
  HANDLE valid_bitmap =
      win_util::GetSectionFromProcess(bitmap, process_->process(), true);
  if (valid_bitmap) {
    PaintBackingStore(valid_bitmap, bitmap_rect);
    CloseHandle(valid_bitmap);
  }
}

void RenderWidgetHost::ResetHangMonitorTimeout() {
  hung_renderer_factory_.RevokeAll();
}
