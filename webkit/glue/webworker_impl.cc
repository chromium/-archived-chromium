// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "base/compiler_specific.h"

#include "GenericWorkerTask.h"
#include "KURL.h"
#include "ScriptExecutionContext.h"
#include "SecurityOrigin.h"
#include "WorkerContext.h"
#include "WorkerThread.h"
#include <wtf/MainThread.h>
#include <wtf/Threading.h>

#undef LOG

#include "base/logging.h"
#include "webkit/api/public/WebString.h"
#include "webkit/api/public/WebURL.h"
#include "webkit/api/public/WebWorkerClient.h"
#include "webkit/glue/glue_util.h"
#include "webkit/glue/webworker_impl.h"

using WebKit::WebWorker;
using WebKit::WebWorkerClient;
using WebKit::WebString;
using WebKit::WebURL;

#if ENABLE(WORKERS)

namespace WebKit {

WebWorker* WebWorker::create(WebWorkerClient* client) {
  return new WebWorkerImpl(client);
}

}

// This function is called on the main thread to force to initialize some static
// values used in WebKit before any worker thread is started. This is because in
// our worker processs, we do not run any WebKit code in main thread and thus
// when multiple workers try to start at the same time, we might hit crash due
// to contention for initializing static values.
void InitializeWebKitStaticValues() {
  static bool initialized = false;
  if (!initialized) {
    initialized= true;
    // Note that we have to pass a URL with valid protocol in order to follow
    // the path to do static value initializations.
    WTF::RefPtr<WebCore::SecurityOrigin> origin =
        WebCore::SecurityOrigin::create(WebCore::KURL("http://localhost"));
    origin.release();
  }
}

WebWorkerImpl::WebWorkerImpl(WebWorkerClient* client) : client_(client) {
  InitializeWebKitStaticValues();
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

  this_ptr->confirmMessageFromWorkerObject(
      worker_context->hasPendingActivity());
}

// WebWorker -------------------------------------------------------------------

void WebWorkerImpl::startWorkerContext(const WebURL& script_url,
                                       const WebString& user_agent,
                                       const WebString& source_code) {
  worker_thread_ = WebCore::WorkerThread::create(
      webkit_glue::WebURLToKURL(script_url),
      webkit_glue::WebStringToString(user_agent),
      webkit_glue::WebStringToString(source_code),
      *this,
      *this);

  // Worker initialization means a pending activity.
  reportPendingActivity(true);

  worker_thread_->start();
}

void WebWorkerImpl::terminateWorkerContext() {
  worker_thread_->stop();
}

void WebWorkerImpl::postMessageToWorkerContext(const WebString& message) {
  worker_thread_->runLoop().postTask(WebCore::createCallbackTask(
      &PostMessageToWorkerContextTask,
      this,
      webkit_glue::WebStringToString(message)));
}

void WebWorkerImpl::workerObjectDestroyed() {
}

void WebWorkerImpl::DispatchTaskToMainThread(
    PassRefPtr<WebCore::ScriptExecutionContext::Task> task) {
  return WTF::callOnMainThread(InvokeTaskMethod, task.releaseRef());
}

void WebWorkerImpl::InvokeTaskMethod(void* param) {
  WebCore::ScriptExecutionContext::Task* task =
      static_cast<WebCore::ScriptExecutionContext::Task*>(param);
  task->performTask(NULL);
  task->deref();
}

// WorkerObjectProxy -----------------------------------------------------------

void WebWorkerImpl::postMessageToWorkerObject(const WebCore::String& message) {
  DispatchTaskToMainThread(WebCore::createCallbackTask(
      &PostMessageTask,
      this,
      message));
}

void WebWorkerImpl::PostMessageTask(
    WebCore::ScriptExecutionContext* context,
    WebWorkerImpl* this_ptr,
    WebCore::String message) {
  this_ptr->client_->postMessageToWorkerObject(
      webkit_glue::StringToWebString(message));
}

void WebWorkerImpl::postExceptionToWorkerObject(
    const WebCore::String& error_message,
    int line_number,
    const WebCore::String& source_url) {
  DispatchTaskToMainThread(WebCore::createCallbackTask(
      &PostExceptionTask,
      this,
      error_message,
      line_number,
      source_url));
}

void WebWorkerImpl::PostExceptionTask(
      WebCore::ScriptExecutionContext* context,
      WebWorkerImpl* this_ptr,
      const WebCore::String& error_message,
      int line_number,
      const WebCore::String& source_url) {
  this_ptr->client_->postExceptionToWorkerObject(
      webkit_glue::StringToWebString(error_message),
      line_number,
      webkit_glue::StringToWebString(source_url));
}

void WebWorkerImpl::postConsoleMessageToWorkerObject(
    WebCore::MessageDestination destination,
    WebCore::MessageSource source,
    WebCore::MessageLevel level,
    const WebCore::String& message,
    int line_number,
    const WebCore::String& source_url) {
  DispatchTaskToMainThread(WebCore::createCallbackTask(
      &PostConsoleMessageTask,
      this,
      static_cast<int>(destination),
      static_cast<int>(source),
      static_cast<int>(level),
      message,
      line_number,
      source_url));
}

void WebWorkerImpl::PostConsoleMessageTask(
    WebCore::ScriptExecutionContext* context,
    WebWorkerImpl* this_ptr,
    int destination,
    int source,
    int level,
    const WebCore::String& message,
    int line_number,
    const WebCore::String& source_url) {
  this_ptr->client_->postConsoleMessageToWorkerObject(
      destination,
      source,
      level,
      webkit_glue::StringToWebString(message),
      line_number,
      webkit_glue::StringToWebString(source_url));
}

void WebWorkerImpl::confirmMessageFromWorkerObject(bool has_pending_activity) {
  DispatchTaskToMainThread(WebCore::createCallbackTask(
      &ConfirmMessageTask,
      this,
      has_pending_activity));
}

void WebWorkerImpl::ConfirmMessageTask(
    WebCore::ScriptExecutionContext* context,
    WebWorkerImpl* this_ptr,
    bool has_pending_activity) {
  this_ptr->client_->confirmMessageFromWorkerObject(has_pending_activity);
}

void WebWorkerImpl::reportPendingActivity(bool has_pending_activity) {
  DispatchTaskToMainThread(WebCore::createCallbackTask(
      &ReportPendingActivityTask,
      this,
      has_pending_activity));
}

void WebWorkerImpl::ReportPendingActivityTask(
    WebCore::ScriptExecutionContext* context,
    WebWorkerImpl* this_ptr,
    bool has_pending_activity) {
  this_ptr->client_->reportPendingActivity(has_pending_activity);
}

void WebWorkerImpl::workerContextDestroyed() {
  DispatchTaskToMainThread(WebCore::createCallbackTask(
      &WorkerContextDestroyedTask,
      this));
}

// WorkerLoaderProxy -----------------------------------------------------------

void WebWorkerImpl::postTaskToLoader(
    PassRefPtr<WebCore::ScriptExecutionContext::Task> task) {
  NOTIMPLEMENTED();
}

void WebWorkerImpl::postTaskForModeToWorkerContext(
    PassRefPtr<WebCore::ScriptExecutionContext::Task> task,
    const WebCore::String& mode) {
  NOTIMPLEMENTED();
}

void WebWorkerImpl::WorkerContextDestroyedTask(
    WebCore::ScriptExecutionContext* context,
    WebWorkerImpl* this_ptr) {
  this_ptr->client_->workerContextDestroyed();

  // The lifetime of this proxy is controlled by the worker context.
  delete this_ptr;
}

#else

namespace WebKit {

WebWorker* WebWorker::create(WebWorkerClient* client) {
  return NULL;
}

}

#endif
