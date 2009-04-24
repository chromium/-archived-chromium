// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#if ENABLE(WORKERS)

#include "webkit/tools/test_shell/test_worker/test_webworker.h"

#include "base/compiler_specific.h"
#include "base/task.h"
#undef LOG
#include "webkit/glue/webworker_impl.h"
#include "webkit/tools/test_shell/test_webworker_helper.h"

using WebKit::WebString;
using WebKit::WebURL;
using WebKit::WebWorker;
using WebKit::WebWorkerClient;

// We use this class to pass a WebString between threads.  This works by
// copying to a string16 which is itself safe to pass between threads.
// TODO(jam): Remove this once all worker callbacks happen on the main thread.
class ThreadSafeWebString {
 public:
  explicit ThreadSafeWebString(const WebString& str)
      : str_(str) {
  }
  operator WebString() const {
    return str_;
  }
 private:
  string16 str_;
};

TestWebWorker::TestWebWorker(WebWorkerClient* client,
                             TestWebWorkerHelper* webworker_helper)
    : webworkerclient_delegate_(client),
      webworker_impl_(NULL),
      webworker_helper_(webworker_helper) {
  AddRef();  // Adds the reference held for worker object.
  AddRef();  // Adds the reference held for worker context object.
}

TestWebWorker::~TestWebWorker() {
  if (webworker_helper_)
    webworker_helper_->Unload();
}

void TestWebWorker::startWorkerContext(const WebURL& script_url,
                                       const WebString& user_agent,
                                       const WebString& source_code) {
  webworker_impl_ = new WebWorkerImpl(this);

  webworker_impl_->startWorkerContext(script_url, user_agent, source_code);

  for (size_t i = 0; i < queued_messages_.size(); ++i)
    webworker_impl_->postMessageToWorkerContext(queued_messages_[i]);
  queued_messages_.clear();
}

void TestWebWorker::terminateWorkerContext() {
  if (webworker_impl_)
    webworker_impl_->terminateWorkerContext();
}

void TestWebWorker::postMessageToWorkerContext(const WebString& message) {
  if (webworker_impl_)
    webworker_impl_->postMessageToWorkerContext(message);
  else
    queued_messages_.push_back(message);
}

void TestWebWorker::workerObjectDestroyed() {
  if (webworker_impl_)
    webworker_impl_->workerObjectDestroyed();

  webworkerclient_delegate_ = NULL;
  Release();    // Releases the reference held for worker object.
}

void TestWebWorker::postMessageToWorkerObject(const WebString& message) {
  if (webworker_helper_->IsMainThread()) {
    if (webworkerclient_delegate_)
      webworkerclient_delegate_->postMessageToWorkerObject(message);
  } else {
    webworker_helper_->DispatchToMainThread(
        InvokeMainThreadMethod, NewRunnableMethod(
            this, &TestWebWorker::postMessageToWorkerObject,
            ThreadSafeWebString(message)));
  }
}

void TestWebWorker::postExceptionToWorkerObject(const WebString& error_message,
                                                int line_number,
                                                const WebString& source_url) {
  if (webworker_helper_->IsMainThread()) {
    if (webworkerclient_delegate_)
      webworkerclient_delegate_->postExceptionToWorkerObject(error_message,
                                                             line_number,
                                                             source_url);
  } else {
    webworker_helper_->DispatchToMainThread(
        InvokeMainThreadMethod, NewRunnableMethod(
            this, &TestWebWorker::postExceptionToWorkerObject,
            ThreadSafeWebString(error_message), line_number,
            ThreadSafeWebString(source_url)));
  }
}

void TestWebWorker::postConsoleMessageToWorkerObject(
    int destination_id,
    int source_id,
    int message_level,
    const WebString& message,
    int line_number,
    const WebString& source_url) {
  if (webworker_helper_->IsMainThread()) {
    if (webworkerclient_delegate_)
      webworkerclient_delegate_->postConsoleMessageToWorkerObject(
          destination_id, source_id, message_level, message, line_number,
          source_url);
  } else {
    webworker_helper_->DispatchToMainThread(
        InvokeMainThreadMethod, NewRunnableMethod(
            this, &TestWebWorker::postConsoleMessageToWorkerObject,
            destination_id, source_id, message_level,
            ThreadSafeWebString(message), line_number,
            ThreadSafeWebString(source_url)));
  }
}

void TestWebWorker::confirmMessageFromWorkerObject(bool has_pending_activity) {
  if (webworker_helper_->IsMainThread()) {
    if (webworkerclient_delegate_)
      webworkerclient_delegate_->confirmMessageFromWorkerObject(
          has_pending_activity);
  } else {
    webworker_helper_->DispatchToMainThread(
        InvokeMainThreadMethod, NewRunnableMethod(
            this, &TestWebWorker::confirmMessageFromWorkerObject,
            has_pending_activity));
  }
}

void TestWebWorker::reportPendingActivity(bool has_pending_activity) {
  if (webworker_helper_->IsMainThread()) {
    if (webworkerclient_delegate_)
      webworkerclient_delegate_->reportPendingActivity(has_pending_activity);
  } else {
    webworker_helper_->DispatchToMainThread(
        InvokeMainThreadMethod, NewRunnableMethod(
            this, &TestWebWorker::reportPendingActivity,
            has_pending_activity));
  }
}

void TestWebWorker::workerContextDestroyed() {
  if (webworker_helper_->IsMainThread()) {
    if (webworkerclient_delegate_)
      webworkerclient_delegate_->workerContextDestroyed();
    Release();    // Releases the reference held for worker context object.
  } else {
    webworker_impl_ = NULL;
    webworker_helper_->DispatchToMainThread(
        InvokeMainThreadMethod, NewRunnableMethod(
            this, &TestWebWorker::workerContextDestroyed));
  }
}

void TestWebWorker::InvokeMainThreadMethod(void* param) {
  Task* task = static_cast<Task*>(param);
  task->Run();
  delete task;
}

#endif
