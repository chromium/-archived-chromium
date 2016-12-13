// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_WORKER_WEBWORKERCLIENT_PROXY_H_
#define CHROME_WORKER_WEBWORKERCLIENT_PROXY_H_

#include "base/basictypes.h"
#include "chrome/common/ipc_channel.h"
#include "googleurl/src/gurl.h"
#include "webkit/api/public/WebWorkerClient.h"

namespace WebKit {
class WebWorker;
}

// This class receives IPCs from the renderer and calls the WebCore::Worker
// implementation (after the data types have been converted by glue code).  It
// is also called by the worker code and converts these function calls into
// IPCs that are sent to the renderer, where they're converted back to function
// calls by WebWorkerProxy.
class WebWorkerClientProxy : public WebKit::WebWorkerClient,
                             public IPC::Channel::Listener {
 public:
  WebWorkerClientProxy(const GURL& url, int route_id);

  // WebWorkerClient implementation.
  virtual void postMessageToWorkerObject(const WebKit::WebString& message);
  virtual void postExceptionToWorkerObject(
      const WebKit::WebString& error_message,
      int line_number,
      const WebKit::WebString& source_url);
  virtual void postConsoleMessageToWorkerObject(
      int destination,
      int source,
      int level,
      const WebKit::WebString& message,
      int line_number,
      const WebKit::WebString& source_url);
  virtual void confirmMessageFromWorkerObject(bool has_pending_activity);
  virtual void reportPendingActivity(bool has_pending_activity);
  virtual void workerContextDestroyed();
  virtual WebKit::WebWorker* createWorker(WebKit::WebWorkerClient* client);

  // IPC::Channel::Listener implementation.
  virtual void OnMessageReceived(const IPC::Message& message);

 private:
  ~WebWorkerClientProxy ();

  bool Send(IPC::Message* message);

  void OnTerminateWorkerContext();

  // The source url for this worker.
  GURL url_;

  int route_id_;

  WebKit::WebWorker* impl_;

  DISALLOW_COPY_AND_ASSIGN(WebWorkerClientProxy);
};

#endif  // CHROME_WORKER_WEBWORKERCLIENT_PROXY_H_
