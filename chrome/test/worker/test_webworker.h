// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_WORKER_TEST_WEBWORKER_H_
#define CHROME_TEST_WORKER_TEST_WEBWORKER_H_

#if ENABLE(WORKERS)

#include <vector>

#include "base/basictypes.h"
#include "base/ref_counted.h"
#include "webkit/glue/webworker.h"
#include "webkit/glue/webworkerclient.h"

class GURL;
class TestWebWorkerHelper;

class TestWebWorker : public WebWorker,
                      public WebWorkerClient,
                      public base::RefCounted<TestWebWorker> {
 public:
  TestWebWorker(WebWorkerClient* client, TestWebWorkerHelper* webworker_helper);

  // WebWorker implementation.
  virtual void StartWorkerContext(const GURL& script_url,
                                  const string16& user_agent,
                                  const string16& source_code);
  virtual void TerminateWorkerContext();
  virtual void PostMessageToWorkerContext(const string16& message);
  virtual void WorkerObjectDestroyed();

  // WebWorkerClient implementation.
  virtual void PostMessageToWorkerObject(const string16& message);
  virtual void PostExceptionToWorkerObject(
      const string16& error_message,
      int line_number,
      const string16& source_url);
  virtual void PostConsoleMessageToWorkerObject(
      int destination,
      int source,
      int level,
      const string16& message,
      int line_number,
      const string16& source_url);
  virtual void ConfirmMessageFromWorkerObject(bool has_pending_activity);
  virtual void ReportPendingActivity(bool has_pending_activity);
  virtual void WorkerContextDestroyed();

 private:
  friend class base::RefCounted<TestWebWorker>;
  virtual ~TestWebWorker();

  static void InvokeMainThreadMethod(void* param);

  WebWorkerClient* webworkerclient_delegate_;
  WebWorker* webworker_impl_;
  TestWebWorkerHelper* webworker_helper_;
  std::vector<string16> queued_messages_;

  DISALLOW_COPY_AND_ASSIGN(TestWebWorker);
};

#endif

#endif  // CHROME_TEST_WORKER_TEST_WEBWORKER_H_
