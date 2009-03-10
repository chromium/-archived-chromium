// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_WEBWORKERCLIENT_IMPL_H_
#define WEBKIT_GLUE_WEBWORKERCLIENT_IMPL_H_

#if ENABLE(WORKERS)

#include "base/scoped_ptr.h"
#include "webkit/glue/webworkerclient.h"

#include "WorkerContextProxy.h"
#include <wtf/RefPtr.h>

class WebWorker;

namespace WebCore {
class ScriptExecutionContext;
};

// The purpose of this class is to provide a WorkerContextProxy
// implementation that we can give to WebKit.  Internally, it converts the
// data types to Chrome compatible ones so that renderer code can use it over
// IPC.
class WebWorkerClientImpl : public WebCore::WorkerContextProxy,
                            public WebWorkerClient {
 public:
  WebWorkerClientImpl(WebCore::Worker* worker);

  void set_webworker(WebWorker* webworker);

  // WebCore::WorkerContextProxy implementation
  void startWorkerContext(const WebCore::KURL& scriptURL,
                          const WebCore::String& userAgent,
                          const WebCore::String& encoding,
                          const WebCore::String& sourceCode);
  void terminateWorkerContext();
  void postMessageToWorkerContext(const WebCore::String& message);
  bool hasPendingActivity() const;
  void workerObjectDestroyed();

  // WebWorkerClient implementation.
  void PostMessageToWorkerObject(const string16& message);
  void PostExceptionToWorkerObject(const string16& error_message,
                                   int line_number,
                                   const string16& source_url);
  void PostConsoleMessageToWorkerObject(int destination,
                                        int source,
                                        int level,
                                        const string16& message,
                                        int line_number,
                                        const string16& source_url);
  void ConfirmMessageFromWorkerObject(bool has_pending_activity);
  void ReportPendingActivity(bool has_pending_activity);
  void WorkerContextDestroyed();

 private:
  virtual ~WebWorkerClientImpl();

  // Guard against context from being destroyed before a worker exits.
  WTF::RefPtr<WebCore::ScriptExecutionContext> script_execution_context_;

  WebCore::Worker* worker_;
  scoped_ptr<WebWorker> webworker_;
  bool asked_to_terminate_;
  uint32 unconfirmed_message_count_;
  bool worker_context_had_pending_activity_;
};

#endif

#endif  // WEBKIT_GLUE_WEBWORKERCLIENT_IMPL_H_
