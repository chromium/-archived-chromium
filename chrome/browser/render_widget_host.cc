// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/render_widget_host.h"

#include "base/gfx/gdi_util.h"
#include "base/message_loop.h"
#include "chrome/app/chrome_dll_resource.h"
#include "chrome/browser/render_widget_helper.h"
#include "chrome/browser/render_widget_host_view.h"
#include "chrome/browser/renderer_host/render_process_host.h"
#include "chrome/common/mru_cache.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/win_util.h"
#include "chrome/views/view.h"
#include "webkit/glue/webcursor.h"
#include "webkit/glue/webinputevent.h"

using base::Time;
using base::TimeDelta;
using base::TimeTicks;

// How long to (synchronously) wait for the renderer to respond with a
// PaintRect message, when our backing-store is invalid, before giving up and
// returning a null or incorrectly sized backing-store from GetBackingStore.
// This timeout impacts the "choppiness" of our window resize perf.
static const int kPaintMsgTimeoutMS = 40;

// How long to wait before we consider a renderer hung.
static const int kHungRendererDelayMs = 20000;

///////////////////////////////////////////////////////////////////////////////
// RenderWidget::BackingStore

RenderWidgetHost::BackingStore::BackingStore(const gfx::Size& size)
    : size_(size),
      backing_store_dib_(NULL),
      original_bitmap_(NULL) {
  HDC screen_dc = ::GetDC(NULL);
  hdc_ = CreateCompatibleDC(screen_dc);
  ReleaseDC(NULL, screen_dc);
}

RenderWidgetHost::BackingStore::~BackingStore() {
  DCHECK(hdc_);

  DeleteDC(hdc_);

  if (backing_store_dib_) {
    DeleteObject(backing_store_dib_);
    backing_store_dib_ = NULL;
  }
}

bool RenderWidgetHost::BackingStore::Refresh(HANDLE process, 
                                             HANDLE bitmap_section,
                                             const gfx::Rect& bitmap_rect) {
  // The bitmap received is valid only in the renderer process.
  HANDLE valid_bitmap =
      win_util::GetSectionFromProcess(bitmap_section, process, false);
  if (!valid_bitmap)
    return false;

  if (!backing_store_dib_) {
    backing_store_dib_ = CreateDIB(hdc_, size_.width(), size_.height(), true,
                                   NULL);
    DCHECK(backing_store_dib_ != NULL);
    original_bitmap_ = SelectObject(hdc_, backing_store_dib_);
  }

  // TODO(darin): protect against integer overflow
  DWORD size = 4 * bitmap_rect.width() * bitmap_rect.height();
  void* backing_store_data = MapViewOfFile(valid_bitmap, FILE_MAP_READ, 0, 0,
                                           size);
  // These values are shared with gfx::PlatformDevice
  BITMAPINFOHEADER hdr;
  gfx::CreateBitmapHeader(bitmap_rect.width(), bitmap_rect.height(), &hdr);
  // Account for a bitmap_rect that exceeds the bounds of our view
  gfx::Rect view_rect(0, 0, size_.width(), size_.height());
  gfx::Rect paint_rect = view_rect.Intersect(bitmap_rect);

  StretchDIBits(hdc_,
                paint_rect.x(),
                paint_rect.y(),
                paint_rect.width(),
                paint_rect.height(),
                0, 0,  // source x,y
                paint_rect.width(),
                paint_rect.height(),
                backing_store_data,
                reinterpret_cast<BITMAPINFO*>(&hdr),
                DIB_RGB_COLORS,
                SRCCOPY);

  UnmapViewOfFile(backing_store_data);
  CloseHandle(valid_bitmap);
  return true;
}

HANDLE RenderWidgetHost::BackingStore::CreateDIB(HDC dc, int width, int height,
                                                 bool use_system_color_depth,
                                                 HANDLE section) {
  BITMAPINFOHEADER hdr;

  if (use_system_color_depth) {
    HDC screen_dc = ::GetDC(NULL);
    int color_depth = GetDeviceCaps(screen_dc, BITSPIXEL);
    ::ReleaseDC(NULL, screen_dc);

    // Color depths less than 16 bpp require a palette to be specified in the
    // BITMAPINFO structure passed to CreateDIBSection. Instead of creating
    // the palette, we specify the desired color depth as 16 which allows the
    // OS to come up with an approximation. Tested this with 8bpp.
    if (color_depth < 16)
      color_depth = 16;

    gfx::CreateBitmapHeaderWithColorDepth(width, height, color_depth, &hdr);
  } else {
    gfx::CreateBitmapHeader(width, height, &hdr);
  }
  void* data = NULL;
  HANDLE dib =
      CreateDIBSection(hdc_, reinterpret_cast<BITMAPINFO*>(&hdr),
                       0, &data, section, 0);
  return dib;
}


///////////////////////////////////////////////////////////////////////////////
// RenderWidgetHost::BackingStoreManager

// This class manages backing stores in the browsr. Every RenderWidgetHost
// is associated with a backing store which it requests from this class.
// The hosts don't maintain any references to the backing stores. 
// These backing stores are maintained in a cache which can be trimmed as
// needed.
class RenderWidgetHost::BackingStoreManager {
 public:
  // Returns a backing store which matches the desired dimensions.
  // Parameters:
  // host
  //  A pointer to the RenderWidgetHost.
  // backing_store_rect
  //  The desired backing store dimensions.
  // Returns a pointer to the backing store on success, NULL on failure.
  static BackingStore* GetBackingStore(RenderWidgetHost* host,
                                       const gfx::Size& desired_size) {
    BackingStore* backing_store = Lookup(host);
    if (backing_store) {
      // If we already have a backing store, then make sure it is the correct
      // size.
      if (backing_store->size() == desired_size)
        return backing_store;
      backing_store = NULL;
    }

    return backing_store;
  }

  // Returns a backing store which is fully ready for consumption,
  // i.e. the bitmap from the renderer has been copied into the
  // backing store dc, or the bitmap in the backing store dc references
  // the renderer bitmap.
  // Parameters:
  // host
  //   A pointer to the RenderWidgetHost.
  // backing_store_rect
  //   The desired backing store dimensions.
  // process_handle
  //   The renderer process handle.
  // bitmap_section
  //   The bitmap section from the renderer.
  // bitmap_rect
  //   The rect to be painted into the backing store
  // needs_full_paint
  //   Set if we need to send out a request to paint the view
  //   to the renderer.
  static BackingStore* PrepareBackingStore(RenderWidgetHost* host, 
                                           const gfx::Rect& backing_store_rect,
                                           HANDLE process_handle,
                                           HANDLE bitmap_section,
                                           const gfx::Rect& bitmap_rect,
                                           bool* needs_full_paint) {
    BackingStore* backing_store = GetBackingStore(host,
                                                  backing_store_rect.size());
    if (!backing_store) {
      // We need to get Webkit to generate a new paint here, as we
      // don't have a previous snapshot.
      if (bitmap_rect != backing_store_rect) {
        DCHECK(needs_full_paint != NULL);
        *needs_full_paint = true;
      }
      backing_store = CreateBackingStore(host, backing_store_rect);
    }

    DCHECK(backing_store != NULL);
    backing_store->Refresh(process_handle, bitmap_section, bitmap_rect);
    return backing_store;
  }

  // Returns a matching backing store for the host.
  // Returns NULL if we fail to find one.
  static BackingStore* Lookup(RenderWidgetHost* host) {
    if (cache_) {
      BackingStoreCache::iterator it = cache_->Peek(host);
      if (it != cache_->end())
        return it->second;
    }
    return NULL;
  }

  // Removes the backing store for the host.
  static void RemoveBackingStore(RenderWidgetHost* host) {
    if (!cache_)
      return;

    BackingStoreCache::iterator it = cache_->Peek(host);
    if (it == cache_->end())
      return;

    cache_->Erase(it);

    if (cache_->empty()) {
      delete cache_;
      cache_ = NULL;
    }
  }

 private:
   // Not intended for instantiation.
  ~BackingStoreManager();

  typedef OwningMRUCache<RenderWidgetHost*, BackingStore*> BackingStoreCache;
  static BackingStoreCache* cache_;

  // Returns the size of the backing store cache.
  // TODO(iyengar) Make this dynamic, i.e. based on the available resources
  // on the machine.
  static int GetBackingStoreCacheSize() {
    const int kMaxSize = 5;
    return kMaxSize;
  }

  // Creates the backing store for the host based on the dimensions passed in.
  // Removes the existing backing store if there is one.
  static BackingStore* CreateBackingStore(
      RenderWidgetHost* host, const gfx::Rect& backing_store_rect) {
    RemoveBackingStore(host);

    BackingStore* backing_store = new BackingStore(backing_store_rect.size());
    int backing_store_cache_size = GetBackingStoreCacheSize();
    if (backing_store_cache_size > 0) {
      if (!cache_)
        cache_ = new BackingStoreCache(backing_store_cache_size);
      cache_->Put(host, backing_store);
    }
    return backing_store;
  }

  DISALLOW_EVIL_CONSTRUCTORS(BackingStoreManager);
};

RenderWidgetHost::BackingStoreManager::BackingStoreCache* 
    RenderWidgetHost::BackingStoreManager::cache_ = NULL;


///////////////////////////////////////////////////////////////////////////////
// RenderWidgetHost

RenderWidgetHost::RenderWidgetHost(RenderProcessHost* process,
                                   int routing_id)
    : process_(process),
      routing_id_(routing_id),
      resize_ack_pending_(false),
      mouse_move_pending_(false),
      view_(NULL),
      is_loading_(false),
      is_hidden_(false),
      suppress_view_updating_(false),
      needs_repainting_on_restore_(false),
      is_unresponsive_(false),
      view_being_painted_(false),
      repaint_ack_pending_(false) {
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
  // Note that we ignore the position.
  view_->SetSize(pos.size());
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
    UMA_HISTOGRAM_TIMES(L"MPArch.RWH_RepaintDelta", delta);
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
  if (view_ && !suppress_view_updating_) {
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
  if (view_) {
    view_being_painted_ = true;
    view_->DidScrollRect(params.clip_rect, params.dx, params.dy);
    view_being_painted_ = false;
  }

  // Log the time delta for processing a scroll message.
  TimeDelta delta = TimeTicks::Now() - scroll_start;
  UMA_HISTOGRAM_TIMES(L"MPArch.RWH_OnMsgScrollRect", delta);
}

void RenderWidgetHost::MovePluginWindows(
    const std::vector<WebPluginGeometry>& plugin_window_moves) {
  if (plugin_window_moves.empty())
    return;

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
    gfx::SubtractRectanglesFromRegion(hrgn, move.cutout_rects);

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
  StopHangMonitorTimeout();

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
                                            const gfx::Rect& caret_rect) {
  if (view_) {
    view_->IMEUpdateStatus(control, caret_rect);
  }
}

///////////////////////////////////////////////////////////////////////////////

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
}

void RenderWidgetHost::WasResized() {
  if (resize_ack_pending_ || !process_->channel() || !view_)
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

  StartHangMonitorTimeout(TimeDelta::FromMilliseconds(kHungRendererDelayMs));
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
  NotificationService::current()->Notify(
      NOTIFY_RENDER_WIDGET_HOST_DESTROYED,
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
  BackingStore* backing_store = 
      BackingStoreManager::GetBackingStore(this, current_size_);
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
      suppress_view_updating_ = true;
      ViewHostMsg_PaintRect::Dispatch(
          &msg, this, &RenderWidgetHost::OnMsgPaintRect);
      suppress_view_updating_ = false;
      backing_store = BackingStoreManager::GetBackingStore(this, current_size_);
    }
  }

  return backing_store;
}

void RenderWidgetHost::PaintRect(HANDLE bitmap, const gfx::Rect& bitmap_rect,
                                 const gfx::Size& view_size) {
  if (is_hidden_) {
    needs_repainting_on_restore_ = true;
    return;
  }

  // We use the view size according to the render view, which may not be
  // quite the same as the size of our window.
  gfx::Rect view_rect(0, 0, view_size.width(), view_size.height());

  bool needs_full_paint = false;
  BackingStore* backing_store = 
      BackingStoreManager::PrepareBackingStore(this, view_rect,
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
  BackingStore* backing_store = BackingStoreManager::Lookup(this);
  if (!backing_store || (backing_store->size() != view_size))
    return;

  RECT damaged_rect, r = clip_rect.ToRECT();
  ScrollDC(backing_store->dc(), dx, dy, NULL, &r, NULL, &damaged_rect);

  // TODO(darin): this doesn't work if dx and dy are both non-zero!
  DCHECK(dx == 0 || dy == 0);

  // We expect that damaged_rect should equal bitmap_rect.
  DCHECK(gfx::Rect(damaged_rect) == bitmap_rect);

  backing_store->Refresh(process_->process().handle(), bitmap, bitmap_rect);
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

void RenderWidgetHost::RendererExited() {
  BackingStoreManager::RemoveBackingStore(this);
}

void RenderWidgetHost::SystemThemeChanged() {
  Send(new ViewMsg_ThemeChanged(routing_id_));
}
