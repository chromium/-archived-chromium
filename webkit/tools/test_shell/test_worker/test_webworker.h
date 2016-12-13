// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_WORKER_TEST_WEBWORKER_H_
#define CHROME_TEST_WORKER_TEST_WEBWORKER_H_

#if ENABLE(WORKERS)

#include <vector>

#include "base/basictypes.h"
#include "base/ref_counted.h"
#include "webkit/api/public/WebString.h"
#include "webkit/api/public/WebWorker.h"
#include "webkit/api/public/WebWorkerClient.h"

class GURL;
class TestWebWorkerHelper;

class TestWebWorker : public WebKit::WebWorker,
                      public WebKit::WebWorkerClient,
                      public base::RefCounted<TestWebWorker> {
 public:
  TestWebWorker(WebWorkerClient* client, TestWebWorkerHelper* webworker_helper);

  // WebWorker methods:
  virtual void startWorkerContext(const WebKit::WebURL& script_url,
                                  const WebKit::WebString& user_agent,
                                  const WebKit::WebString& source_code);
  virtual void terminateWorkerContext();
  virtual void postMessageToWorkerContext(const WebKit::WebString&);
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
  virtual WebKit::WebWorker* createWorker(WebKit::WebWorkerClient* client) {
    return NULL;
  }

 private:
  friend class base::RefCounted<TestWebWorker>;
  virtual ~TestWebWorker();

  static void InvokeMainThreadMethod(void* param);

  WebKit::WebWorkerClient* webworkerclient_delegate_;
  WebKit::WebWorker* webworker_impl_;
  TestWebWorkerHelper* webworker_helper_;
  std::vector<string16> queued_messages_;

  DISALLOW_COPY_AND_ASSIGN(TestWebWorker);
};

#endif

#endif  // CHROME_TEST_WORKER_TEST_WEBWORKER_H_
