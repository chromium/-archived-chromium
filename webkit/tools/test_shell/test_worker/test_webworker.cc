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
  if (!webworkerclient_delegate_)
    return;
  // The string was created in the dll's memory space as a result of a postTask.
  // If we pass it to test shell's memory space, it'll cause problems when GC
  // occurs.  So duplicate it from the test shell's memory space first.
  webworkerclient_delegate_->postMessageToWorkerObject(
      webworker_helper_->DuplicateString(message));
}

void TestWebWorker::postExceptionToWorkerObject(const WebString& error_message,
                                                int line_number,
                                                const WebString& source_url) {
  if (webworkerclient_delegate_)
    webworkerclient_delegate_->postExceptionToWorkerObject(error_message,
                                                           line_number,
                                                           source_url);
}

void TestWebWorker::postConsoleMessageToWorkerObject(
    int destination_id,
    int source_id,
    int message_level,
    const WebString& message,
    int line_number,
    const WebString& source_url) {
  if (webworkerclient_delegate_)
    webworkerclient_delegate_->postConsoleMessageToWorkerObject(
        destination_id, source_id, message_level, message, line_number,
        source_url);
}

void TestWebWorker::confirmMessageFromWorkerObject(bool has_pending_activity) {
  if (webworkerclient_delegate_)
    webworkerclient_delegate_->confirmMessageFromWorkerObject(
        has_pending_activity);
}

void TestWebWorker::reportPendingActivity(bool has_pending_activity) {
  if (webworkerclient_delegate_)
    webworkerclient_delegate_->reportPendingActivity(has_pending_activity);
}

void TestWebWorker::workerContextDestroyed() {
  webworker_impl_ = NULL;
  if (webworkerclient_delegate_)
    webworkerclient_delegate_->workerContextDestroyed();
  Release();    // Releases the reference held for worker context object.
}

#endif
