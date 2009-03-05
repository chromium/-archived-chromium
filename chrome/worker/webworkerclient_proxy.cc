// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/worker/webworkerclient_proxy.h"

#include "chrome/common/ipc_logging.h"
#include "chrome/common/worker_messages.h"
#include "chrome/worker/worker_process.h"
#include "chrome/worker/worker_thread.h"
#include "webkit/glue/webworker.h"


WebWorkerClientProxy::WebWorkerClientProxy(const GURL& url, int route_id)
    : url_(url),
      route_id_(route_id),
      started_worker_(false),
      ALLOW_THIS_IN_INITIALIZER_LIST(impl_(WebWorker::Create(this))) {
  WorkerThread::current()->AddRoute(route_id_, this);
  WorkerProcess::current()->AddRefProcess();
}

WebWorkerClientProxy::~WebWorkerClientProxy() {
  WorkerThread::current()->RemoveRoute(route_id_);
  WorkerProcess::current()->ReleaseProcess();
}

void WebWorkerClientProxy::PostMessageToWorkerObject(const string16& message) {
  Send(new WorkerHostMsg_PostMessageToWorkerObject(route_id_, message));
}

void WebWorkerClientProxy::PostExceptionToWorkerObject(
    const string16& error_message,
    int line_number,
    const string16& source_url) {
  Send(new WorkerHostMsg_PostExceptionToWorkerObject(
      route_id_, error_message, line_number, source_url));
}

void WebWorkerClientProxy::PostConsoleMessageToWorkerObject(
    int destination,
    int source,
    int level,
    const string16& message,
    int line_number,
    const string16& source_url) {
  Send(new WorkerHostMsg_PostConsoleMessageToWorkerObject(
      route_id_, destination, source, level,message, line_number, source_url));
}

void WebWorkerClientProxy::ConfirmMessageFromWorkerObject(bool has_pending_activity) {
  Send(new WorkerHostMsg_ConfirmMessageFromWorkerObject(
      route_id_, has_pending_activity));
}

void WebWorkerClientProxy::ReportPendingActivity(bool has_pending_activity) {
  Send(new WorkerHostMsg_ReportPendingActivity(
      route_id_, has_pending_activity));
}

void WebWorkerClientProxy::WorkerContextDestroyed() {
  Send(new WorkerHostMsg_WorkerContextDestroyed(route_id_));
}

bool WebWorkerClientProxy::Send(IPC::Message* message) {
  return WorkerThread::current()->Send(message);
}

void WebWorkerClientProxy::OnMessageReceived(const IPC::Message& message) {
  if (!started_worker_ &&
      message.type() != WorkerMsg_StartWorkerContext::ID) {
    queued_messages_.push_back(new IPC::Message(message));
    return;
  }

  WebWorker* worker = impl_.get();
  IPC_BEGIN_MESSAGE_MAP(WebWorkerClientProxy, message)
    IPC_MESSAGE_FORWARD(WorkerMsg_StartWorkerContext, worker,
                        WebWorker::StartWorkerContext)
    IPC_MESSAGE_FORWARD(WorkerMsg_TerminateWorkerContext, worker,
                        WebWorker::TerminateWorkerContext)
    IPC_MESSAGE_FORWARD(WorkerMsg_PostMessageToWorkerContext, worker,
                        WebWorker::PostMessageToWorkerContext)
    IPC_MESSAGE_FORWARD(WorkerMsg_WorkerObjectDestroyed, worker,
                        WebWorker::WorkerObjectDestroyed)
  IPC_END_MESSAGE_MAP()

  if (message.type() == WorkerMsg_StartWorkerContext::ID) {
    started_worker_ = true;
    for (size_t i = 0; i < queued_messages_.size(); ++i)
      OnMessageReceived(*queued_messages_[i]);

    queued_messages_.clear();
  }
}
