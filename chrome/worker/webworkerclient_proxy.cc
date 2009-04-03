// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/worker/webworkerclient_proxy.h"

#include "chrome/common/child_process.h"
#include "chrome/common/ipc_logging.h"
#include "chrome/common/worker_messages.h"
#include "chrome/worker/worker_thread.h"
#include "webkit/glue/webworker.h"


WebWorkerClientProxy::WebWorkerClientProxy(const GURL& url, int route_id)
    : url_(url),
      route_id_(route_id),
      ALLOW_THIS_IN_INITIALIZER_LIST(impl_(WebWorker::Create(this))) {
  AddRef();
  WorkerThread::current()->AddRoute(route_id_, this);
  ChildProcess::current()->AddRefProcess();
}

WebWorkerClientProxy::~WebWorkerClientProxy() {
  WorkerThread::current()->RemoveRoute(route_id_);
  ChildProcess::current()->ReleaseProcess();
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

void WebWorkerClientProxy::ConfirmMessageFromWorkerObject(
    bool has_pending_activity) {
  Send(new WorkerHostMsg_ConfirmMessageFromWorkerObject(
      route_id_, has_pending_activity));
}

void WebWorkerClientProxy::ReportPendingActivity(bool has_pending_activity) {
  Send(new WorkerHostMsg_ReportPendingActivity(
      route_id_, has_pending_activity));
}

void WebWorkerClientProxy::WorkerContextDestroyed() {
  Send(new WorkerHostMsg_WorkerContextDestroyed(route_id_));
  impl_ = NULL;

  WorkerThread::current()->message_loop()->ReleaseSoon(FROM_HERE, this);
}

bool WebWorkerClientProxy::Send(IPC::Message* message) {
  if (MessageLoop::current() != WorkerThread::current()->message_loop()) {
    WorkerThread::current()->message_loop()->PostTask(FROM_HERE,
        NewRunnableMethod(this, &WebWorkerClientProxy::Send, message));
    return true;
  }

  return WorkerThread::current()->Send(message);
}

void WebWorkerClientProxy::OnMessageReceived(const IPC::Message& message) {
  if (!impl_)
    return;

  IPC_BEGIN_MESSAGE_MAP(WebWorkerClientProxy, message)
    IPC_MESSAGE_FORWARD(WorkerMsg_StartWorkerContext, impl_,
                        WebWorker::StartWorkerContext)
    IPC_MESSAGE_FORWARD(WorkerMsg_TerminateWorkerContext, impl_,
                        WebWorker::TerminateWorkerContext)
    IPC_MESSAGE_FORWARD(WorkerMsg_PostMessageToWorkerContext, impl_,
                        WebWorker::PostMessageToWorkerContext)
    IPC_MESSAGE_FORWARD(WorkerMsg_WorkerObjectDestroyed, impl_,
                        WebWorker::WorkerObjectDestroyed)
  IPC_END_MESSAGE_MAP()
}
