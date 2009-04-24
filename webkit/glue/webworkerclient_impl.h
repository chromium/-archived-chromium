// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_WEBWORKERCLIENT_IMPL_H_
#define WEBKIT_GLUE_WEBWORKERCLIENT_IMPL_H_

#if ENABLE(WORKERS)

#include "third_party/WebKit/WebKit/chromium/public/WebWorkerClient.h"

#include "WorkerContextProxy.h"
#include <wtf/RefPtr.h>

namespace WebCore {
class ScriptExecutionContext;
}
namespace WebKit {
class WebWorker;
}

// The purpose of this class is to provide a WorkerContextProxy
// implementation that we can give to WebKit.  Internally, it converts the
// data types to Chrome compatible ones so that renderer code can use it over
// IPC.
class WebWorkerClientImpl : public WebCore::WorkerContextProxy,
                            public WebKit::WebWorkerClient {
 public:
  WebWorkerClientImpl(WebCore::Worker* worker);

  void set_webworker(WebKit::WebWorker* webworker);

  // WebCore::WorkerContextProxy methods:
  virtual void startWorkerContext(const WebCore::KURL& script_url,
                                  const WebCore::String& user_agent,
                                  const WebCore::String& source_code);
  virtual void terminateWorkerContext();
  virtual void postMessageToWorkerContext(const WebCore::String& message);
  virtual bool hasPendingActivity() const;
  virtual void workerObjectDestroyed();

  // WebWorkerClient methods:
  virtual void postMessageToWorkerObject(const WebKit::WebString& message);
  virtual void postExceptionToWorkerObject(
      const WebKit::WebString& error_message,
      int line_number,
      const WebKit::WebString& source_url);
  virtual void postConsoleMessageToWorkerObject(
      int destination_id,
      int source_id,
      int message_level,
      const WebKit::WebString& message,
      int line_number,
      const WebKit::WebString& source_url);
  virtual void confirmMessageFromWorkerObject(bool has_pending_activity);
  virtual void reportPendingActivity(bool has_pending_activity);
  virtual void workerContextDestroyed();

 private:
  virtual ~WebWorkerClientImpl();

  // Guard against context from being destroyed before a worker exits.
  WTF::RefPtr<WebCore::ScriptExecutionContext> script_execution_context_;

  WebCore::Worker* worker_;
  WebKit::WebWorker* webworker_;
  bool asked_to_terminate_;
  uint32 unconfirmed_message_count_;
  bool worker_context_had_pending_activity_;
};

#endif

#endif  // WEBKIT_GLUE_WEBWORKERCLIENT_IMPL_H_
