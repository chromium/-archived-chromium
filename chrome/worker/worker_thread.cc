// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/worker/worker_thread.h"

#include "chrome/common/worker_messages.h"
#include "chrome/worker/webworkerclient_proxy.h"
#include "chrome/worker/worker_process.h"

WorkerThread::WorkerThread()
    : ChildThread(base::Thread::Options(MessageLoop::TYPE_DEFAULT, kV8StackSize)) {
}

WorkerThread::~WorkerThread() {
}

void WorkerThread::OnControlMessageReceived(const IPC::Message& msg) {
  IPC_BEGIN_MESSAGE_MAP(WorkerThread, msg)
    IPC_MESSAGE_HANDLER(WorkerProcessMsg_CreateWorker, OnCreateWorker)
  IPC_END_MESSAGE_MAP()
}

void WorkerThread::OnCreateWorker(const GURL& url, int route_id) {
  WebWorkerClientProxy* worker = new WebWorkerClientProxy(url, route_id);
}
