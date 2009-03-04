// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/webworker_proxy.h"

#include "chrome/common/render_messages.h"
#include "chrome/common/worker_messages.h"
#include "chrome/renderer/render_thread.h"
#include "webkit/glue/webworkerclient.h"

WebWorkerProxy::WebWorkerProxy(
    IPC::Message::Sender* sender, WebWorkerClient* client)
    : sender_(sender),
      route_id_(MSG_ROUTING_NONE),
      client_(client) {
}

WebWorkerProxy::~WebWorkerProxy() {
}

void WebWorkerProxy::StartWorkerContext(
    const GURL& script_url,
    const string16& user_agent,
    const string16& source_code) {
  sender_->Send(
      new ViewHostMsg_CreateDedicatedWorker(script_url, &route_id_));
  if (route_id_ == MSG_ROUTING_NONE)
    return;

  RenderThread::current()->AddRoute(route_id_, this);
  Send(new WorkerMsg_StartWorkerContext(
      route_id_, script_url, user_agent, source_code));
}

void WebWorkerProxy::TerminateWorkerContext() {
  if (route_id_ != MSG_ROUTING_NONE) {
    Send(new WorkerMsg_TerminateWorkerContext(route_id_));
    RenderThread::current()->RemoveRoute(route_id_);
    route_id_ = MSG_ROUTING_NONE;
  }
}

void WebWorkerProxy::PostMessageToWorkerContext(
    const string16& message) {
  Send(new WorkerMsg_PostMessageToWorkerContext(route_id_, message));
}

void WebWorkerProxy::WorkerObjectDestroyed() {
  client_ = NULL;
  Send(new WorkerMsg_WorkerObjectDestroyed(route_id_));
}

bool WebWorkerProxy::Send(IPC::Message* message) {
  if (route_id_ == MSG_ROUTING_NONE) {
    delete message;
    return false;
  }

  // For now we proxy all messages to the worker process through the browser.
  // Revisit if we find this slow.
  // TODO(jabdelmalek): handle sync messages if we need them.
  IPC::Message* wrapped_msg = new ViewHostMsg_ForwardToWorker(*message);
  delete message;
  return sender_->Send(wrapped_msg);
}

void WebWorkerProxy::OnMessageReceived(const IPC::Message& message) {
  if (!client_)
    return;

  IPC_BEGIN_MESSAGE_MAP(WebWorkerProxy, message)
    IPC_MESSAGE_FORWARD(WorkerHostMsg_PostMessageToWorkerObject,
                        client_,
                        WebWorkerClient::PostMessageToWorkerObject)
    IPC_MESSAGE_FORWARD(WorkerHostMsg_PostExceptionToWorkerObject,
                        client_,
                        WebWorkerClient::PostExceptionToWorkerObject)
    IPC_MESSAGE_FORWARD(WorkerHostMsg_PostConsoleMessageToWorkerObject,
                        client_,
                        WebWorkerClient::PostConsoleMessageToWorkerObject)
    IPC_MESSAGE_FORWARD(WorkerHostMsg_ConfirmMessageFromWorkerObject,
                        client_,
                        WebWorkerClient::ConfirmMessageFromWorkerObject)
    IPC_MESSAGE_FORWARD(WorkerHostMsg_ReportPendingActivity,
                        client_,
                        WebWorkerClient::ReportPendingActivity)
    IPC_MESSAGE_FORWARD(WorkerHostMsg_WorkerContextDestroyed,
                        client_,
                        WebWorkerClient::WorkerContextDestroyed)
  IPC_END_MESSAGE_MAP()
}
