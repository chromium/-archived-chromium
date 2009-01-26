// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_host/render_widget_helper.h"

#include "base/thread.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/renderer_host/render_process_host.h"
#include "chrome/browser/renderer_host/resource_dispatcher_host.h"

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

RenderWidgetHelper::RenderWidgetHelper(int render_process_id)
    : render_process_id_(render_process_id),
      ui_loop_(MessageLoop::current()),
      event_(CreateEvent(NULL, FALSE /* auto-reset */, FALSE, NULL)),
      block_popups_(false) {
}

RenderWidgetHelper::~RenderWidgetHelper() {
  // The elements of pending_paints_ each hold an owning reference back to this
  // object, so we should not be destroyed unless pending_paints_ is empty!
  DCHECK(pending_paints_.empty());
}

int RenderWidgetHelper::GetNextRoutingID() {
  return next_routing_id_.GetNext() + 1;
}

void RenderWidgetHelper::CancelResourceRequests(int render_widget_id) {
  if (g_browser_process->io_thread()) {
    g_browser_process->io_thread()->message_loop()->PostTask(FROM_HERE,
        NewRunnableMethod(this,
                          &RenderWidgetHelper::OnCancelResourceRequests,
                          g_browser_process->resource_dispatcher_host(),
                          render_widget_id));
  }
}

void RenderWidgetHelper::CrossSiteClosePageACK(int new_render_process_host_id,
                                               int new_request_id) {
  if (g_browser_process->io_thread()) {
    g_browser_process->io_thread()->message_loop()->PostTask(FROM_HERE,
        NewRunnableMethod(this,
                          &RenderWidgetHelper::OnCrossSiteClosePageACK,
                          g_browser_process->resource_dispatcher_host(),
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
    ResourceDispatcherHost* dispatcher,
    int render_widget_id) {
  dispatcher->CancelRequestsForRenderView(render_process_id_, render_widget_id);
}

void RenderWidgetHelper::OnCrossSiteClosePageACK(
    ResourceDispatcherHost* dispatcher,
    int new_render_process_host_id,
    int new_request_id) {
  dispatcher->OnClosePageACK(new_render_process_host_id, new_request_id);
}

void RenderWidgetHelper::CreateNewWindow(int opener_id,
                                         bool user_gesture,
                                         base::ProcessHandle render_process,
                                         int* route_id
#if defined(OS_WIN)
                                         , HANDLE* modal_dialog_event) {
#else
                                         ) {
#endif
  if (!user_gesture && block_popups_) {
    *route_id = MSG_ROUTING_NONE;
    *modal_dialog_event = NULL;
    return;
  }

  *route_id = GetNextRoutingID();

#if defined(OS_WIN)
  HANDLE event = CreateEvent(NULL, TRUE, FALSE, NULL);
  BOOL result = DuplicateHandle(GetCurrentProcess(),
                                event,
                                render_process,
                                modal_dialog_event,
                                SYNCHRONIZE,
                                FALSE,
                                0);
  DCHECK(result) << "Couldn't duplicate modal dialog event for the renderer.";

  // The easiest way to reach RenderViewHost is just to send a routed message.
  ViewHostMsg_CreateWindowWithRoute msg(opener_id, *route_id, event);
#else
  // TODO(port) figure out how the modal dialog event should work.
  ViewHostMsg_CreateWindowWithRoute msg(opener_id, *route_id);
#endif

  ui_loop_->PostTask(FROM_HERE, NewRunnableMethod(
      this, &RenderWidgetHelper::OnSimulateReceivedMessage, msg));
}

void RenderWidgetHelper::CreateNewWidget(int opener_id,
                                         bool activatable,
                                         int* route_id) {
  *route_id = GetNextRoutingID();
  ViewHostMsg_CreateWidgetWithRoute msg(opener_id, *route_id, activatable);
  ui_loop_->PostTask(FROM_HERE, NewRunnableMethod(
      this, &RenderWidgetHelper::OnSimulateReceivedMessage, msg));
}

void RenderWidgetHelper::OnSimulateReceivedMessage(
    const IPC::Message& message) {
  RenderProcessHost* host = RenderProcessHost::FromID(render_process_id_);
  if (host)
    host->OnMessageReceived(message);
}
