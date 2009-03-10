// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_WEBWORKER_IMPL_H_
#define WEBKIT_GLUE_WEBWORKER_IMPL_H_

#include "webkit/glue/webworker.h"

#if ENABLE(WORKERS)

#include <vector>
#include "ScriptExecutionContext.h"
#include "WorkerObjectProxy.h"
#include <wtf/RefPtr.h>

namespace WebCore {
class ScriptExecutionContext;
class Strng;
class WorkerThread;
};

// This class is used by the worker process code to talk to the WebCore::Worker
// implementation.  It can't use it directly since it uses WebKit types, so this
// class converts the data types.  When the WebCore::Worker object wants to call
// WebCore::WorkerObjectProxy, this class will conver to Chrome data types first
// and then call the supplied WebWorkerClient.
class WebWorkerImpl: public WebCore::WorkerObjectProxy,
                     public WebWorker {
 public:
  WebWorkerImpl(WebWorkerClient* client);
  virtual ~WebWorkerImpl();

  // WebCore::WorkerObjectProxy implementation.
  void postMessageToWorkerObject(const WebCore::String& message);
  void postExceptionToWorkerObject(const WebCore::String& errorMessage,
                                   int lineNumber,
                                   const WebCore::String& sourceURL);
  void postConsoleMessageToWorkerObject(WebCore::MessageDestination destination,
                                        WebCore::MessageSource source,
                                        WebCore::MessageLevel level,
                                        const WebCore::String& message,
                                        int lineNumber,
                                        const WebCore::String& sourceURL);
  void confirmMessageFromWorkerObject(bool hasPendingActivity);
  void reportPendingActivity(bool hasPendingActivity);
  void workerContextDestroyed();

  // WebWorker implementation.
  void StartWorkerContext(const GURL& script_url,
                          const string16& user_agent,
                          const string16& encoding,
                          const string16& source_code);
  void TerminateWorkerContext();
  void PostMessageToWorkerContext(const string16& message);
  void WorkerObjectDestroyed();

 private:
  static void PostMessageToWorkerContextTask(
      WebCore::ScriptExecutionContext* context,
      WebWorkerImpl* this_ptr,
      const WebCore::String& message);

  WebWorkerClient* client_;
  WTF::RefPtr<WebCore::WorkerThread> worker_thread_;

  DISALLOW_COPY_AND_ASSIGN(WebWorkerImpl);
};

#endif

#endif  // WEBKIT_GLUE_WEBWORKER_IMPL_H_
