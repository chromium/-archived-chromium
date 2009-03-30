// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#if ENABLE(WORKERS)

#include "chrome/test/worker/test_webworker.h"

#include "base/compiler_specific.h"
#include "base/task.h"
#undef LOG
#include "webkit/glue/webworkerclient.h"
#include "webkit/glue/webworker_impl.h"
#include "webkit/tools/test_shell/test_webworker_helper.h"

TestWebWorker::TestWebWorker(WebWorkerClient* client,
                             TestWebWorkerHelper* webworker_helper)
    : webworkerclient_delegate_(client),
      webworker_impl_(NULL),
      webworker_helper_(webworker_helper) {
  AddRef();   // Adds the reference held for worker object.
  AddRef();   // Adds the reference held for worker context object.
}

TestWebWorker::~TestWebWorker() {
  if (webworker_helper_)
    webworker_helper_->Unload();
}

void TestWebWorker::StartWorkerContext(const GURL& script_url,
                                       const string16& user_agent,
                                       const string16& source_code) {
  webworker_impl_ = new WebWorkerImpl(this);

  webworker_impl_->StartWorkerContext(script_url,
                                      user_agent,
                                      source_code);

  for (size_t i = 0; i < queued_messages_.size(); ++i)
    webworker_impl_->PostMessageToWorkerContext(queued_messages_[i]);
  queued_messages_.clear();
}

void TestWebWorker::TerminateWorkerContext() {
  if (webworker_impl_)
    webworker_impl_->TerminateWorkerContext();
}

void TestWebWorker::PostMessageToWorkerContext(const string16& message) {
  if (webworker_impl_)
      webworker_impl_->PostMessageToWorkerContext(message);
  else
    queued_messages_.push_back(message);
}

void TestWebWorker::WorkerObjectDestroyed() {
  if (webworker_impl_)
    webworker_impl_->WorkerObjectDestroyed();

  webworkerclient_delegate_ = NULL;
  Release();    // Releases the reference held for worker object.
}

void TestWebWorker::PostMessageToWorkerObject(const string16& message) {
  if (webworker_helper_->IsMainThread()) {
    if (webworkerclient_delegate_)
      webworkerclient_delegate_->PostMessageToWorkerObject(message);
  } else {
    webworker_helper_->DispatchToMainThread(
        InvokeMainThreadMethod, NewRunnableMethod(
            this, &TestWebWorker::PostMessageToWorkerObject, message));
  }
}

void TestWebWorker::PostExceptionToWorkerObject(const string16& error_message,
                                                int line_number,
                                                const string16& source_url) {
  if (webworker_helper_->IsMainThread()) {
    if (webworkerclient_delegate_)
      webworkerclient_delegate_->PostExceptionToWorkerObject(error_message,
                                                             line_number,
                                                             source_url);
  } else {
    webworker_helper_->DispatchToMainThread(
        InvokeMainThreadMethod, NewRunnableMethod(
            this, &TestWebWorker::PostExceptionToWorkerObject,
            error_message, line_number, source_url));
  }
}

void TestWebWorker::PostConsoleMessageToWorkerObject(
    int destination,
    int source,
    int level,
    const string16& message,
    int line_number,
    const string16& source_url) {
  if (webworker_helper_->IsMainThread()) {
    if (webworkerclient_delegate_)
      webworkerclient_delegate_->PostConsoleMessageToWorkerObject(destination,
                                                                  source,
                                                                  level,
                                                                  message,
                                                                  line_number,
                                                                  source_url);
  } else {
    webworker_helper_->DispatchToMainThread(
        InvokeMainThreadMethod, NewRunnableMethod(
            this, &TestWebWorker::PostConsoleMessageToWorkerObject,
            destination, source, level, message, line_number, source_url));
  }
}

void TestWebWorker::ConfirmMessageFromWorkerObject(bool has_pending_activity) {
  if (webworker_helper_->IsMainThread()) {
    if (webworkerclient_delegate_)
      webworkerclient_delegate_->ConfirmMessageFromWorkerObject(
          has_pending_activity);
  } else {
    webworker_helper_->DispatchToMainThread(
        InvokeMainThreadMethod, NewRunnableMethod(
            this, &TestWebWorker::ConfirmMessageFromWorkerObject,
            has_pending_activity));
  }
}

void TestWebWorker::ReportPendingActivity(bool has_pending_activity) {
  if (webworker_helper_->IsMainThread()) {
    if (webworkerclient_delegate_)
      webworkerclient_delegate_->ReportPendingActivity(has_pending_activity);
  } else {
    webworker_helper_->DispatchToMainThread(
        InvokeMainThreadMethod, NewRunnableMethod(
            this, &TestWebWorker::ReportPendingActivity,
            has_pending_activity));
  }
}

void TestWebWorker::WorkerContextDestroyed() {
  if (webworker_helper_->IsMainThread()) {
    if (webworkerclient_delegate_)
      webworkerclient_delegate_->WorkerContextDestroyed();
    Release();    // Releases the reference held for worker context object.
  } else {
    webworker_impl_ = NULL;
    webworker_helper_->DispatchToMainThread(
        InvokeMainThreadMethod, NewRunnableMethod(
            this, &TestWebWorker::WorkerContextDestroyed));
  }
}

void TestWebWorker::InvokeMainThreadMethod(void* param) {
  Task* task = static_cast<Task*>(param);
  task->Run();
  delete task;
}

#endif
