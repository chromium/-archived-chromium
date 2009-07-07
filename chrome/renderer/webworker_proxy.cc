// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/webworker_proxy.h"

#include "chrome/common/child_thread.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/worker_messages.h"
#include "webkit/api/public/WebURL.h"
#include "webkit/api/public/WebWorkerClient.h"

using WebKit::WebString;
using WebKit::WebURL;
using WebKit::WebWorkerClient;

WebWorkerProxy::WebWorkerProxy(
    WebWorkerClient* client,
    ChildThread* child_thread,
    int render_view_route_id)
    : route_id_(MSG_ROUTING_NONE),
      child_thread_(child_thread),
      render_view_route_id_(render_view_route_id),
      client_(client) {
}

WebWorkerProxy::~WebWorkerProxy() {
  Disconnect();

  for (size_t i = 0; i < queued_messages_.size(); ++i)
    delete queued_messages_[i];
}

void WebWorkerProxy::Disconnect() {
  if (route_id_ == MSG_ROUTING_NONE)
    return;

  // So the messages from WorkerContext (like WorkerContextDestroyed) do not
  // come after nobody is listening. Since Worker and WorkerContext can
  // terminate independently, already sent messages may still be in the pipe.
  child_thread_->RemoveRoute(route_id_);

  // Tell the browser to not start our queued worker.
  if (!queued_messages_.empty())
    child_thread_->Send(new ViewHostMsg_CancelCreateDedicatedWorker(route_id_));

  route_id_ = MSG_ROUTING_NONE;
}

void WebWorkerProxy::startWorkerContext(
    const WebURL& script_url,
    const WebString& user_agent,
    const WebString& source_code) {
  child_thread_->Send(new ViewHostMsg_CreateDedicatedWorker(
      script_url, render_view_route_id_, &route_id_));
  if (route_id_ == MSG_ROUTING_NONE)
    return;

  child_thread_->AddRoute(route_id_, this);

  // We make sure that the start message is the first, since postMessage might
  // have already been called.
  queued_messages_.insert(queued_messages_.begin(),
      new WorkerMsg_StartWorkerContext(
          route_id_, script_url, user_agent, source_code));
}

void WebWorkerProxy::terminateWorkerContext() {
  if (route_id_ != MSG_ROUTING_NONE) {
    Send(new WorkerMsg_TerminateWorkerContext(route_id_));
    Disconnect();
  }
}

void WebWorkerProxy::postMessageToWorkerContext(
    const WebString& message) {
  Send(new WorkerMsg_PostMessageToWorkerContext(route_id_, message));
}

void WebWorkerProxy::workerObjectDestroyed() {
  Send(new WorkerMsg_WorkerObjectDestroyed(route_id_));
  delete this;
}

bool WebWorkerProxy::Send(IPC::Message* message) {
  // It's possible that postMessage is called before the worker is created, in
  // which case route_id_ will be none.  Or the worker object can be interacted
  // with before the browser process told us that it started, in which case we
  // also want to queue the message.
  if (route_id_ == MSG_ROUTING_NONE || !queued_messages_.empty()) {
    queued_messages_.push_back(message);
    return true;
  }

  // For now we proxy all messages to the worker process through the browser.
  // Revisit if we find this slow.
  // TODO(jabdelmalek): handle sync messages if we need them.
  IPC::Message* wrapped_msg = new ViewHostMsg_ForwardToWorker(*message);
  delete message;
  return child_thread_->Send(wrapped_msg);
}

void WebWorkerProxy::OnMessageReceived(const IPC::Message& message) {
  if (!client_)
    return;

  IPC_BEGIN_MESSAGE_MAP(WebWorkerProxy, message)
    IPC_MESSAGE_HANDLER(ViewMsg_DedicatedWorkerCreated,
                        OnDedicatedWorkerCreated)
    IPC_MESSAGE_FORWARD(WorkerHostMsg_PostMessageToWorkerObject,
                        client_,
                        WebWorkerClient::postMessageToWorkerObject)
    IPC_MESSAGE_FORWARD(WorkerHostMsg_PostExceptionToWorkerObject,
                        client_,
                        WebWorkerClient::postExceptionToWorkerObject)
    IPC_MESSAGE_FORWARD(WorkerHostMsg_PostConsoleMessageToWorkerObject,
                        client_,
                        WebWorkerClient::postConsoleMessageToWorkerObject)
    IPC_MESSAGE_FORWARD(WorkerHostMsg_ConfirmMessageFromWorkerObject,
                        client_,
                        WebWorkerClient::confirmMessageFromWorkerObject)
    IPC_MESSAGE_FORWARD(WorkerHostMsg_ReportPendingActivity,
                        client_,
                        WebWorkerClient::reportPendingActivity)
    IPC_MESSAGE_FORWARD(WorkerHostMsg_WorkerContextDestroyed,
                        client_,
                        WebWorkerClient::workerContextDestroyed)
  IPC_END_MESSAGE_MAP()
}

void WebWorkerProxy::OnDedicatedWorkerCreated() {
  DCHECK(queued_messages_.size());
  std::vector<IPC::Message*> queued_messages = queued_messages_;
  queued_messages_.clear();
  for (size_t i = 0; i < queued_messages.size(); ++i) {
    queued_messages[i]->set_routing_id(route_id_);
    Send(queued_messages[i]);
  }
}
