// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "base/compiler_specific.h"
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

void WebWorkerImpl::StartWorkerContext(const GURL& script_url,
                                       const string16& user_agent,
                                       const string16& source_code) {
  // TODO(jianli): implement WorkerContextProxy here (i.e. create WorkerThread
  // etc).  The WebKit code uses worker_object_proxy_ when it wants to talk to
  // code running in the renderer process.
}

void WebWorkerImpl::TerminateWorkerContext() {
}

void WebWorkerImpl::PostMessageToWorkerContext(const string16& message) {
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
}

#else

WebWorker* WebWorker::Create(WebWorkerClient* client) {
  return NULL;
}

#endif
