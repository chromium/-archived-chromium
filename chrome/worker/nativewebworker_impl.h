// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_WORKER_NATIVEWORKER_H_
#define CHROME_WORKER_NATIVEWORKER_H_

#include "base/basictypes.h"
#include "webkit/api/public/WebWorker.h"
#include "webkit/api/public/WebWorkerClient.h"


// Forward declaration for the listener thread pointer.
class NativeWebWorkerListenerThread;

// This class is used by the worker process code to talk to the Native Client
// worker implementation.
class NativeWebWorkerImpl : public WebKit::WebWorker {
 public:
  explicit NativeWebWorkerImpl(WebKit::WebWorkerClient* client);
  virtual ~NativeWebWorkerImpl();

  static WebWorker* NativeWebWorkerImpl::create(
            WebKit::WebWorkerClient* client);

  // WebWorker implementation.
  void startWorkerContext(const WebKit::WebURL& script_url,
                          const WebKit::WebString& user_agent,
                          const WebKit::WebString& source_code);
  void terminateWorkerContext();
  void postMessageToWorkerContext(const WebKit::WebString& message);
  void workerObjectDestroyed();

 private:
  WebKit::WebWorkerClient* client_;
  struct NaClApp* nap_;
  struct NaClSrpcChannel* channel_;
  NativeWebWorkerListenerThread* upcall_thread_;
  struct NaClDesc* descs_[2];

  DISALLOW_COPY_AND_ASSIGN(NativeWebWorkerImpl);
};

#endif  // CHROME_WORKER_NATIVEWEBWORKER_H_
