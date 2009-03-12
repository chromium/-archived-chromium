// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_host/render_widget_helper.h"

#include "base/thread.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/renderer_host/render_process_host.h"
#include "chrome/browser/renderer_host/resource_dispatcher_host.h"
#include "chrome/common/render_messages.h"

// A Task used with InvokeLater that we hold a pointer to in pending_paints_.
// Instances are deleted by MessageLoop after it calls their Run method.
class RenderWidgetHelper::PaintMsgProxy : public Task {
 public:
  PaintMsgProxy(RenderWidgetHelper* h, const IPC::Message& m)
      : helper(h),
        message(m),
        cancelled(false) {
  }

  ~PaintMsgProxy() {
    // If the paint message was never dispatched, then we need to let the
    // helper know that we are going away.
    if (!cancelled && helper)
      helper->OnDiscardPaintMsg(this);
  }

  virtual void Run() {
    if (!cancelled) {
      helper->OnDispatchPaintMsg(this);
      helper = NULL;
    }
  }

  scoped_refptr<RenderWidgetHelper> helper;
  IPC::Message message;
  bool cancelled;  // If true, then the message will not be dispatched.

  DISALLOW_COPY_AND_ASSIGN(PaintMsgProxy);
};

RenderWidgetHelper::RenderWidgetHelper()
    : render_process_id_(-1),
      ui_loop_(MessageLoop::current()),
#if defined(OS_WIN)
      event_(CreateEvent(NULL, FALSE /* auto-reset */, FALSE, NULL)),
#elif defined(OS_POSIX)
      event_(false /* auto-reset */, false),
#endif
      block_popups_(false),
      resource_dispatcher_host_(NULL) {
}

RenderWidgetHelper::~RenderWidgetHelper() {
  // The elements of pending_paints_ each hold an owning reference back to this
  // object, so we should not be destroyed unless pending_paints_ is empty!
  DCHECK(pending_paints_.empty());

#if defined(OS_MACOSX)
  ClearAllocatedDIBs();
#endif
}

void RenderWidgetHelper::Init(
    int render_process_id,
    ResourceDispatcherHost* resource_dispatcher_host) {
  render_process_id_ = render_process_id;
  resource_dispatcher_host_ = resource_dispatcher_host;
}

int RenderWidgetHelper::GetNextRoutingID() {
  return next_routing_id_.GetNext() + 1;
}

void RenderWidgetHelper::CancelResourceRequests(int render_widget_id) {
  if (g_browser_process->io_thread() && render_process_id_ != -1) {
    g_browser_process->io_thread()->message_loop()->PostTask(FROM_HERE,
        NewRunnableMethod(this,
                          &RenderWidgetHelper::OnCancelResourceRequests,
                          render_widget_id));
  }
}

void RenderWidgetHelper::CrossSiteClosePageACK(int new_render_process_host_id,
                                               int new_request_id) {
  if (g_browser_process->io_thread()) {
    g_browser_process->io_thread()->message_loop()->PostTask(FROM_HERE,
        NewRunnableMethod(this,
                          &RenderWidgetHelper::OnCrossSiteClosePageACK,
                          new_render_process_host_id,
                          new_request_id));
  }
}

bool RenderWidgetHelper::WaitForPaintMsg(int render_widget_id,
                                         const base::TimeDelta& max_delay,
                                         IPC::Message* msg) {
  base::TimeTicks time_start = base::TimeTicks::Now();

  for (;;) {
    PaintMsgProxy* proxy = NULL;
    {
      AutoLock lock(pending_paints_lock_);

      PaintMsgProxyMap::iterator it = pending_paints_.find(render_widget_id);
      if (it != pending_paints_.end()) {
        proxy = it->second;

        // Flag the proxy as cancelled so that when it is run as a task it will
        // do nothing.
        proxy->cancelled = true;

        pending_paints_.erase(it);
      }
    }

    if (proxy) {
      *msg = proxy->message;
      DCHECK(msg->routing_id() == render_widget_id);
      return true;
    }

    // Calculate the maximum amount of time that we are willing to sleep.
    base::TimeDelta max_sleep_time =
        max_delay - (base::TimeTicks::Now() - time_start);
    if (max_sleep_time <= base::TimeDelta::FromMilliseconds(0))
      break;

    event_.TimedWait(max_sleep_time);
  }

  return false;
}

void RenderWidgetHelper::DidReceivePaintMsg(const IPC::Message& msg) {
  int render_widget_id = msg.routing_id();

  PaintMsgProxy* proxy;
  {
    AutoLock lock(pending_paints_lock_);

    PaintMsgProxyMap::value_type new_value(render_widget_id, NULL);

    // We expect only a single PaintRect message at a time.  Optimize for the
    // case that we don't already have an entry by using the 'insert' method.
    std::pair<PaintMsgProxyMap::iterator, bool> result =
        pending_paints_.insert(new_value);
    if (!result.second) {
      NOTREACHED() << "Unexpected PaintRect message!";
      return;
    }

    result.first->second = (proxy = new PaintMsgProxy(this, msg));
  }

  // Notify anyone waiting on the UI thread that there is a new entry in the
  // proxy map.  If they don't find the entry they are looking for, then they
  // will just continue waiting.
  event_.Signal();

  // The proxy will be deleted when it is run as a task.
  ui_loop_->PostTask(FROM_HERE, proxy);
}

void RenderWidgetHelper::OnDiscardPaintMsg(PaintMsgProxy* proxy) {
  const IPC::Message& msg = proxy->message;

  // Remove the proxy from the map now that we are going to handle it normally.
  {
    AutoLock lock(pending_paints_lock_);

    PaintMsgProxyMap::iterator it = pending_paints_.find(msg.routing_id());
    DCHECK(it != pending_paints_.end());
    DCHECK(it->second == proxy);

    pending_paints_.erase(it);
  }
}

void RenderWidgetHelper::OnDispatchPaintMsg(PaintMsgProxy* proxy) {
  OnDiscardPaintMsg(proxy);

  // It is reasonable for the host to no longer exist.
  RenderProcessHost* host = RenderProcessHost::FromID(render_process_id_);
  if (host)
    host->OnMessageReceived(proxy->message);
}

void RenderWidgetHelper::OnCancelResourceRequests(
    int render_widget_id) {
  resource_dispatcher_host_->CancelRequestsForRoute(
      render_process_id_, render_widget_id);
}

void RenderWidgetHelper::OnCrossSiteClosePageACK(
    int new_render_process_host_id,
    int new_request_id) {
  resource_dispatcher_host_->OnClosePageACK(
      new_render_process_host_id, new_request_id);
}

void RenderWidgetHelper::CreateNewWindow(int opener_id,
                                         bool user_gesture,
                                         base::ProcessHandle render_process,
                                         int* route_id,
                                         ModalDialogEvent* modal_dialog_event) {
  if (!user_gesture && block_popups_) {
    *route_id = MSG_ROUTING_NONE;
#if defined(OS_WIN)
    modal_dialog_event->event = NULL;
#endif
    return;
  }

  *route_id = GetNextRoutingID();

  ModalDialogEvent modal_dialog_event_internal;
#if defined(OS_WIN)
  HANDLE event = CreateEvent(NULL, TRUE, FALSE, NULL);
  modal_dialog_event_internal.event = event;

  BOOL result = DuplicateHandle(GetCurrentProcess(),
                                event,
                                render_process,
                                &modal_dialog_event->event,
                                SYNCHRONIZE,
                                FALSE,
                                0);
  DCHECK(result) << "Couldn't duplicate modal dialog event for the renderer.";
#endif

  // Block resource requests until the view is created, since the HWND might be
  // needed if a response ends up creating a plugin.
  resource_dispatcher_host_->BlockRequestsForRoute(
      render_process_id_, *route_id);

  // The easiest way to reach RenderViewHost is just to send a routed message.
  ViewHostMsg_CreateWindowWithRoute msg(opener_id, *route_id,
                                        modal_dialog_event_internal);

  ui_loop_->PostTask(FROM_HERE, NewRunnableMethod(
      this, &RenderWidgetHelper::OnCreateWindowOnUI, msg, *route_id));
}

void RenderWidgetHelper::OnCreateWindowOnUI(
    const IPC::Message& message, int route_id) {
  RenderProcessHost* host = RenderProcessHost::FromID(render_process_id_);
  if (host)
    host->OnMessageReceived(message);

  g_browser_process->io_thread()->message_loop()->PostTask(FROM_HERE,
      NewRunnableMethod(this, &RenderWidgetHelper::OnCreateWindowOnIO, route_id));
}

void RenderWidgetHelper::OnCreateWindowOnIO(int route_id) {
  resource_dispatcher_host_->ResumeBlockedRequestsForRoute(
      render_process_id_, route_id);
}

void RenderWidgetHelper::CreateNewWidget(int opener_id,
                                         bool activatable,
                                         int* route_id) {
  *route_id = GetNextRoutingID();
  ViewHostMsg_CreateWidgetWithRoute msg(opener_id, *route_id, activatable);
  ui_loop_->PostTask(FROM_HERE, NewRunnableMethod(
      this, &RenderWidgetHelper::OnCreateWidgetOnUI, msg));
}

void RenderWidgetHelper::OnCreateWidgetOnUI(
    const IPC::Message& message) {
  RenderProcessHost* host = RenderProcessHost::FromID(render_process_id_);
  if (host)
    host->OnMessageReceived(message);
}

#if defined(OS_MACOSX)
TransportDIB* RenderWidgetHelper::MapTransportDIB(TransportDIB::Id dib_id) {
  AutoLock locked(allocated_dibs_lock_);

  const std::map<TransportDIB::Id, int>::iterator
      i = allocated_dibs_.find(dib_id);
  if (i == allocated_dibs_.end())
    return NULL;

  base::FileDescriptor fd(dup(i->second), true);
  return TransportDIB::Map(fd);
}

void RenderWidgetHelper::AllocTransportDIB(
    size_t size, TransportDIB::Handle* result) {
  base::SharedMemory* shared_memory = new base::SharedMemory();
  if (!shared_memory->Create(L"", false /* read write */,
                             false /* do not open existing */, size)) {
    result->fd = -1;
    result->auto_close = false;
    delete shared_memory;
    return;
  }

  shared_memory->GiveToProcess(0 /* pid, not needed */, result);

  // Keep a copy of the file descriptor around
  AutoLock locked(allocated_dibs_lock_);
  allocated_dibs_[shared_memory->id()] = dup(result->fd);
}

void RenderWidgetHelper::FreeTransportDIB(TransportDIB::Id dib_id) {
  AutoLock locked(allocated_dibs_lock_);

  const std::map<TransportDIB::Id, int>::iterator
    i = allocated_dibs_.find(dib_id);

  if (i != allocated_dibs_.end()) {
    close(i->second);
    allocated_dibs_.erase(i);
  } else {
    DLOG(WARNING) << "Renderer asked us to free unknown transport DIB";
  }
}

void RenderWidgetHelper::ClearAllocatedDIBs() {
  for (std::map<TransportDIB::Id, int>::iterator
       i = allocated_dibs_.begin(); i != allocated_dibs_.end(); ++i) {
    close(i->second);
  }

  allocated_dibs_.clear();
}
#endif
