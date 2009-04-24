// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#if ENABLE(WORKERS)

#include "base/compiler_specific.h"

#include "Frame.h"
#include "FrameLoaderClient.h"
#include "ScriptExecutionContext.h"
#include "WorkerMessagingProxy.h"
#include "Worker.h"

#undef LOG

#include "webkit/glue/webworkerclient_impl.h"

#include "webkit/glue/glue_util.h"
#include "webkit/glue/webframeloaderclient_impl.h"
#include "webkit/glue/webframe_impl.h"
#include "webkit/glue/webview_delegate.h"
#include "webkit/glue/webview_impl.h"
#include "third_party/WebKit/WebKit/chromium/public/WebConsoleMessage.h"
#include "third_party/WebKit/WebKit/chromium/public/WebScriptSource.h"
#include "third_party/WebKit/WebKit/chromium/public/WebWorker.h"

using WebKit::WebConsoleMessage;
using WebKit::WebScriptSource;
using WebKit::WebString;
using WebKit::WebWorker;
using WebKit::WebWorkerClient;

// When WebKit creates a WorkerContextProxy object, we check if we're in the
// renderer or worker process.  If the latter, then we just use
// WebCore::WorkerMessagingProxy.
//
// If we're in the renderer process, then we need use the glue provided
// WebWorker object to talk to the worker process over IPC.  The worker process
// talks to WebCore::Worker* using WorkerObjectProxy, which we implement on
// WebWorkerClientImpl.
WebCore::WorkerContextProxy* WebCore::WorkerContextProxy::create(
    WebCore::Worker* worker) {
  if (!worker->scriptExecutionContext()->isDocument())
    return new WebCore::WorkerMessagingProxy(worker);

  WebWorkerClientImpl* proxy = new WebWorkerClientImpl(worker);

  // Get to the RenderView, so that we can tell the browser to create a
  // worker process if necessary.
  WebCore::Document* document = static_cast<WebCore::Document*>(
      worker->scriptExecutionContext());
  WebFrameLoaderClient* frame_loader_client =
      static_cast<WebFrameLoaderClient*>(document->frame()->loader()->client());
  WebViewDelegate* webview_delegate =
      frame_loader_client->webframe()->webview_impl()->delegate();
  WebWorker* webworker = webview_delegate->CreateWebWorker(proxy);
  proxy->set_webworker(webworker);
  return proxy;
}


WebWorkerClientImpl::WebWorkerClientImpl(WebCore::Worker* worker)
    : script_execution_context_(worker->scriptExecutionContext()),
      worker_(worker),
      asked_to_terminate_(false),
      unconfirmed_message_count_(0),
      worker_context_had_pending_activity_(false) {
}

WebWorkerClientImpl::~WebWorkerClientImpl() {
}

void WebWorkerClientImpl::set_webworker(WebWorker* webworker) {
  webworker_ = webworker;
}

void WebWorkerClientImpl::startWorkerContext(
    const WebCore::KURL& script_url,
    const WebCore::String& user_agent,
    const WebCore::String& source_code) {
  // Worker.terminate() could be called from JS before the context is started.
  if (asked_to_terminate_)
      return;

  webworker_->startWorkerContext(
      webkit_glue::KURLToWebURL(script_url),
      webkit_glue::StringToWebString(user_agent),
      webkit_glue::StringToWebString(source_code));
}

void WebWorkerClientImpl::terminateWorkerContext() {
  if (asked_to_terminate_)
      return;
  asked_to_terminate_ = true;

  webworker_->terminateWorkerContext();
}

void WebWorkerClientImpl::postMessageToWorkerContext(
    const WebCore::String& message) {
  // Worker.terminate() could be called from JS before the context is started.
  if (asked_to_terminate_)
      return;

  ++unconfirmed_message_count_;
  webworker_->postMessageToWorkerContext(
      webkit_glue::StringToWebString(message));
}

bool WebWorkerClientImpl::hasPendingActivity() const {
  return !asked_to_terminate_ &&
         (unconfirmed_message_count_ || worker_context_had_pending_activity_);
}

void WebWorkerClientImpl::workerObjectDestroyed() {
  webworker_->workerObjectDestroyed();

  // The lifetime of this proxy is controlled by the worker.
  delete this;
}

void WebWorkerClientImpl::postMessageToWorkerObject(const WebString& message) {
  worker_->dispatchMessage(webkit_glue::WebStringToString(message));
}

void WebWorkerClientImpl::postExceptionToWorkerObject(
    const WebString& error_message,
    int line_number,
    const WebString& source_url) {
  script_execution_context_->reportException(
      webkit_glue::WebStringToString(error_message),
      line_number,
      webkit_glue::WebStringToString(source_url));
}

void WebWorkerClientImpl::postConsoleMessageToWorkerObject(
    int destination_id,
    int source_id,
    int message_level,
    const WebString& message,
    int line_number,
    const WebString& source_url) {
  script_execution_context_->addMessage(
      static_cast<WebCore::MessageDestination>(destination_id),
      static_cast<WebCore::MessageSource>(source_id),
      static_cast<WebCore::MessageLevel>(message_level),
      webkit_glue::WebStringToString(message),
      line_number,
      webkit_glue::WebStringToString(source_url));
}

void WebWorkerClientImpl::confirmMessageFromWorkerObject(
    bool has_pending_activity) {
  --unconfirmed_message_count_;
}

void WebWorkerClientImpl::reportPendingActivity(bool has_pending_activity) {
  worker_context_had_pending_activity_ = has_pending_activity;
}

void WebWorkerClientImpl::workerContextDestroyed() {
}

#endif
