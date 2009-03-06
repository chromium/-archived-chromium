// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "base/compiler_specific.h"

#include "GenericWorkerTask.h"
#include "ScriptExecutionContext.h"
#include "WorkerContext.h"
#include "WorkerThread.h"
#include <wtf/Threading.h>

#undef LOG

#include "base/logging.h"
#include "webkit/glue/glue_util.h"
#include "webkit/glue/webworkerclient.h"
#include "webkit/glue/webworker_impl.h"


#if ENABLE(WORKERS)

WebWorker* WebWorker::Create(WebWorkerClient* client) {
  return new WebWorkerImpl(client);
}


WebWorkerImpl::WebWorkerImpl(WebWorkerClient* client) : client_(client) {
}

WebWorkerImpl::~WebWorkerImpl() {
}

void WebWorkerImpl::PostMessageToWorkerContextTask(
    WebCore::ScriptExecutionContext* context,
    WebWorkerImpl* this_ptr,
    const WebCore::String& message) {
  DCHECK(context->isWorkerContext());
  WebCore::WorkerContext* worker_context =
      static_cast<WebCore::WorkerContext*>(context);
  worker_context->dispatchMessage(message);

  this_ptr->client_->ConfirmMessageFromWorkerObject(
      worker_context->hasPendingActivity());
}

void WebWorkerImpl::StartWorkerContext(const GURL& script_url,
                                       const string16& user_agent,
                                       const string16& source_code) {
  worker_thread_ = WebCore::WorkerThread::create(
      webkit_glue::GURLToKURL(script_url),
      webkit_glue::String16ToString(user_agent),
      webkit_glue::String16ToString(source_code),
      this);

  // Worker initialization means a pending activity.
  reportPendingActivity(true);

  worker_thread_->start();
}

void WebWorkerImpl::TerminateWorkerContext() {
  worker_thread_->stop();
}

void WebWorkerImpl::PostMessageToWorkerContext(const string16& message) {
  worker_thread_->runLoop().postTask(WebCore::createCallbackTask(
      &PostMessageToWorkerContextTask,
      this,
      webkit_glue::String16ToString(message)));
}

void WebWorkerImpl::WorkerObjectDestroyed() {
}

void WebWorkerImpl::postMessageToWorkerObject(const WebCore::String& message) {
  client_->PostMessageToWorkerObject(webkit_glue::StringToString16(message));
}

void WebWorkerImpl::postExceptionToWorkerObject(
    const WebCore::String& errorMessage,
    int lineNumber,
    const WebCore::String& sourceURL) {
  client_->PostExceptionToWorkerObject(
      webkit_glue::StringToString16(errorMessage),
      lineNumber,
      webkit_glue::StringToString16(sourceURL));
}

void WebWorkerImpl::postConsoleMessageToWorkerObject(
    WebCore::MessageDestination destination,
    WebCore::MessageSource source,
    WebCore::MessageLevel level,
    const WebCore::String& message,
    int lineNumber,
    const WebCore::String& sourceURL) {
  client_->PostConsoleMessageToWorkerObject(
      destination,
      source,
      level,
      webkit_glue::StringToString16(message),
      lineNumber,
      webkit_glue::StringToString16(sourceURL));
}

void WebWorkerImpl::confirmMessageFromWorkerObject(bool hasPendingActivity) {
  client_->ConfirmMessageFromWorkerObject(hasPendingActivity);
}

void WebWorkerImpl::reportPendingActivity(bool hasPendingActivity) {
  client_->ReportPendingActivity(hasPendingActivity);
}

void WebWorkerImpl::workerContextDestroyed() {
  client_->WorkerContextDestroyed();

  // The lifetime of this proxy is controlled by the worker context.
  delete this;
}

#else

WebWorker* WebWorker::Create(WebWorkerClient* client) {
  return NULL;
}

#endif
