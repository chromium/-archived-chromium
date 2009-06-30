// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_WEBWORKER_IMPL_H_
#define WEBKIT_GLUE_WEBWORKER_IMPL_H_

#include "webkit/api/public/WebWorker.h"

#if ENABLE(WORKERS)

#include "ScriptExecutionContext.h"
#include "WorkerLoaderProxy.h"
#include "WorkerObjectProxy.h"
#include <wtf/PassOwnPtr.h>
#include <wtf/RefPtr.h>

namespace WebCore {
class Strng;
class MessagePortChannel;
class WorkerThread;
};

class WebView;

// This class is used by the worker process code to talk to the WebCore::Worker
// implementation.  It can't use it directly since it uses WebKit types, so this
// class converts the data types.  When the WebCore::Worker object wants to call
// WebCore::WorkerObjectProxy, this class will conver to Chrome data types first
// and then call the supplied WebWorkerClient.
class WebWorkerImpl: public WebCore::WorkerObjectProxy,
                     public WebCore::WorkerLoaderProxy,
                     public WebKit::WebWorker {
 public:
  explicit WebWorkerImpl(WebKit::WebWorkerClient* client);

  // WebCore::WorkerObjectProxy methods:
  virtual void postMessageToWorkerObject(
      const WebCore::String& message,
      WTF::PassOwnPtr<WebCore::MessagePortChannel> channel);
  virtual void postExceptionToWorkerObject(
      const WebCore::String& error_message,
      int line_number,
      const WebCore::String& source_url);
  virtual void postConsoleMessageToWorkerObject(
      WebCore::MessageDestination destination,
      WebCore::MessageSource source,
      WebCore::MessageLevel level,
      const WebCore::String& message,
      int line_number,
      const WebCore::String& source_url);
  virtual void confirmMessageFromWorkerObject(bool has_pending_activity);
  virtual void reportPendingActivity(bool has_pending_activity);
  virtual void workerContextDestroyed();

  // WebCore::WorkerLoaderProxy methods:
  virtual void postTaskToLoader(
      WTF::PassRefPtr<WebCore::ScriptExecutionContext::Task>);
  virtual void postTaskForModeToWorkerContext(
      WTF::PassRefPtr<WebCore::ScriptExecutionContext::Task>,
      const WebCore::String& mode);

  // WebWorker methods:
  virtual void startWorkerContext(const WebKit::WebURL& script_url,
                                  const WebKit::WebString& user_agent,
                                  const WebKit::WebString& source_code);
  virtual void terminateWorkerContext();
  virtual void postMessageToWorkerContext(const WebKit::WebString& message);
  virtual void workerObjectDestroyed();

  WebKit::WebWorkerClient* client() { return client_; }

  // Executes the given task on the main thread.
  static void DispatchTaskToMainThread(
      PassRefPtr<WebCore::ScriptExecutionContext::Task> task);

 private:
  virtual ~WebWorkerImpl();

  // Tasks that are run on the worker thread.
  static void PostMessageToWorkerContextTask(
      WebCore::ScriptExecutionContext* context,
      WebWorkerImpl* this_ptr,
      const WebCore::String& message,
      WTF::PassOwnPtr<WebCore::MessagePortChannel> channel);

  // Function used to invoke tasks on the main thread.
  static void InvokeTaskMethod(void* param);

  // Tasks that are run on the main thread.
  static void PostMessageTask(
      WebCore::ScriptExecutionContext* context,
      WebWorkerImpl* this_ptr,
      WebCore::String message,
      WTF::PassOwnPtr<WebCore::MessagePortChannel> channel);
  static void PostExceptionTask(
      WebCore::ScriptExecutionContext* context,
      WebWorkerImpl* this_ptr,
      const WebCore::String& error_message,
      int line_number,
      const WebCore::String& source_url);
  static void PostConsoleMessageTask(
      WebCore::ScriptExecutionContext* context,
      WebWorkerImpl* this_ptr,
      int destination,
      int source,
      int level,
      const WebCore::String& message,
      int line_number,
      const WebCore::String& source_url);
  static void ConfirmMessageTask(
      WebCore::ScriptExecutionContext* context,
      WebWorkerImpl* this_ptr,
      bool has_pending_activity);
  static void ReportPendingActivityTask(
      WebCore::ScriptExecutionContext* context,
      WebWorkerImpl* this_ptr,
      bool has_pending_activity);
  static void WorkerContextDestroyedTask(
      WebCore::ScriptExecutionContext* context,
      WebWorkerImpl* this_ptr);

  WebKit::WebWorkerClient* client_;

  // 'shadow page' - created to proxy loading requests from the worker.
  WTF::RefPtr<WebCore::ScriptExecutionContext> loading_document_;
  WebView* web_view_;

  WTF::RefPtr<WebCore::WorkerThread> worker_thread_;

  DISALLOW_COPY_AND_ASSIGN(WebWorkerImpl);
};

#endif

#endif  // WEBKIT_GLUE_WEBWORKER_IMPL_H_
