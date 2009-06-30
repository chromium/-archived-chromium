// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_WEBWORKERCLIENT_IMPL_H_
#define WEBKIT_GLUE_WEBWORKERCLIENT_IMPL_H_

#if ENABLE(WORKERS)

#include "webkit/api/public/WebWorkerClient.h"

#include "WorkerContextProxy.h"
#include <wtf/PassOwnPtr.h>
#include <wtf/RefPtr.h>

namespace WebCore {
class MessagePortChannel;
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
  // These are called on the thread that created the worker.  In the renderer
  // process, this will be the main WebKit thread.  In the worker process, this
  // will be the thread of the executing worker (not the main WebKit thread).
  virtual void startWorkerContext(const WebCore::KURL& script_url,
                                  const WebCore::String& user_agent,
                                  const WebCore::String& source_code);
  virtual void terminateWorkerContext();
  virtual void postMessageToWorkerContext(
      const WebCore::String& message,
      WTF::PassOwnPtr<WebCore::MessagePortChannel> channel);
  virtual bool hasPendingActivity() const;
  virtual void workerObjectDestroyed();

  // WebWorkerClient methods:
  // These are called on the main WebKit thread.
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
  virtual WebKit::WebWorker* createWorker(WebKit::WebWorkerClient* client) {
    return NULL;
  }

 private:
  virtual ~WebWorkerClientImpl();

  // Methods used to support WebWorkerClientImpl being constructed on worker
  // threads.
  // These tasks are dispatched on the WebKit thread.
  static void StartWorkerContextTask(
      WebCore::ScriptExecutionContext* context,
      WebWorkerClientImpl* this_ptr,
      const WebCore::String& script_url,
      const WebCore::String& user_agent,
      const WebCore::String& source_code);
  static void TerminateWorkerContextTask(
      WebCore::ScriptExecutionContext* context,
      WebWorkerClientImpl* this_ptr);
  static void PostMessageToWorkerContextTask(
      WebCore::ScriptExecutionContext* context,
      WebWorkerClientImpl* this_ptr,
      const WebCore::String& message,
      WTF::PassOwnPtr<WebCore::MessagePortChannel> channel);
  static void WorkerObjectDestroyedTask(
      WebCore::ScriptExecutionContext* context,
      WebWorkerClientImpl* this_ptr);

  // These tasks are dispatched on the thread that created the worker (i.e.
  // main WebKit thread in renderer process, and the worker thread in the worker
  // process).
  static void PostMessageToWorkerObjectTask(
      WebCore::ScriptExecutionContext* context,
      WebWorkerClientImpl* this_ptr,
      const WebCore::String& message,
      WTF::PassOwnPtr<WebCore::MessagePortChannel> channel);
  static void PostExceptionToWorkerObjectTask(
      WebCore::ScriptExecutionContext* context,
      WebWorkerClientImpl* this_ptr,
      const WebCore::String& error_message,
      int line_number,
      const WebCore::String& source_url);
  static void PostConsoleMessageToWorkerObjectTask(
      WebCore::ScriptExecutionContext* context,
      WebWorkerClientImpl* this_ptr,
      int destination_id,
      int source_id,
      int message_level,
      const WebCore::String& message,
      int line_number,
      const WebCore::String& source_url);
  static void ConfirmMessageFromWorkerObjectTask(
      WebCore::ScriptExecutionContext* context,
      WebWorkerClientImpl* this_ptr);
  static void ReportPendingActivityTask(
      WebCore::ScriptExecutionContext* context,
      WebWorkerClientImpl* this_ptr,
      bool has_pending_activity);

  // Guard against context from being destroyed before a worker exits.
  WTF::RefPtr<WebCore::ScriptExecutionContext> script_execution_context_;

  WebCore::Worker* worker_;
  WebKit::WebWorker* webworker_;
  bool asked_to_terminate_;
  uint32 unconfirmed_message_count_;
  bool worker_context_had_pending_activity_;
  WTF::ThreadIdentifier worker_thread_id_;
};

#endif

#endif  // WEBKIT_GLUE_WEBWORKERCLIENT_IMPL_H_
