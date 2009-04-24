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
#include <wtf/Threading.h>

#undef LOG

#include "base/logging.h"
#include "webkit/glue/glue_util.h"
#include "webkit/glue/webworker_impl.h"
#include "third_party/WebKit/WebKit/chromium/public/WebString.h"
#include "third_party/WebKit/WebKit/chromium/public/WebURL.h"
#include "third_party/WebKit/WebKit/chromium/public/WebWorkerClient.h"

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

  this_ptr->client_->confirmMessageFromWorkerObject(
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
      this);

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

// WorkerObjectProxy -----------------------------------------------------------

void WebWorkerImpl::postMessageToWorkerObject(const WebCore::String& message) {
  client_->postMessageToWorkerObject(webkit_glue::StringToWebString(message));
}

void WebWorkerImpl::postExceptionToWorkerObject(
    const WebCore::String& error_message,
    int line_number,
    const WebCore::String& source_url) {
  client_->postExceptionToWorkerObject(
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
  client_->postConsoleMessageToWorkerObject(
      static_cast<int>(destination),
      static_cast<int>(source),
      static_cast<int>(level),
      webkit_glue::StringToWebString(message),
      line_number,
      webkit_glue::StringToWebString(source_url));
}

void WebWorkerImpl::confirmMessageFromWorkerObject(bool has_pending_activity) {
  client_->confirmMessageFromWorkerObject(has_pending_activity);
}

void WebWorkerImpl::reportPendingActivity(bool has_pending_activity) {
  client_->reportPendingActivity(has_pending_activity);
}

void WebWorkerImpl::workerContextDestroyed() {
  client_->workerContextDestroyed();

  // The lifetime of this proxy is controlled by the worker context.
  delete this;
}

#else

namespace WebKit {

WebWorker* WebWorker::create(WebWorkerClient* client) {
  return NULL;
}

}

#endif
