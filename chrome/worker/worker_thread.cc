// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/worker/worker_thread.h"

#include "base/lazy_instance.h"
#include "base/thread_local.h"
#include "chrome/common/worker_messages.h"
#include "chrome/worker/webworkerclient_proxy.h"
#include "chrome/worker/worker_webkitclient_impl.h"
#include "webkit/api/public/WebKit.h"

static base::LazyInstance<base::ThreadLocalPointer<WorkerThread> > lazy_tls(
    base::LINKER_INITIALIZED);


WorkerThread::WorkerThread()
    : ChildThread(base::Thread::Options(MessageLoop::TYPE_DEFAULT,
                                        kV8StackSize)) {
}

WorkerThread::~WorkerThread() {
}

WorkerThread* WorkerThread::current() {
  return lazy_tls.Pointer()->Get();
}

void WorkerThread::Init() {
  lazy_tls.Pointer()->Set(this);
  ChildThread::Init();
  webkit_client_.reset(new WorkerWebKitClientImpl);
  WebKit::initialize(webkit_client_.get());
}

void WorkerThread::CleanUp() {
  // Shutdown in reverse of the initialization order.

  if (webkit_client_.get()) {
    WebKit::shutdown();
    webkit_client_.reset();
  }

  ChildThread::CleanUp();
  lazy_tls.Pointer()->Set(NULL);
}

void WorkerThread::OnControlMessageReceived(const IPC::Message& msg) {
  IPC_BEGIN_MESSAGE_MAP(WorkerThread, msg)
    IPC_MESSAGE_HANDLER(WorkerProcessMsg_CreateWorker, OnCreateWorker)
  IPC_END_MESSAGE_MAP()
}

void WorkerThread::OnCreateWorker(const GURL& url, int route_id) {
  // WebWorkerClientProxy owns itself.
  WebWorkerClientProxy* worker = new WebWorkerClientProxy(url, route_id);
}
