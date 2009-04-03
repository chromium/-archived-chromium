// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_WORKER_WEBWORKERCLIENT_PROXY_H_
#define CHROME_WORKER_WEBWORKERCLIENT_PROXY_H_

#include "base/basictypes.h"
#include "base/ref_counted.h"
#include "ipc/ipc_channel.h"
#include "googleurl/src/gurl.h"
#include "webkit/glue/webworkerclient.h"

class WebWorker;

// This class receives IPCs from the renderer and calls the WebCore::Worker
// implementation (after the data types have been converted by glue code).  It
// is also called by the worker code and converts these function calls into
// IPCs that are sent to the renderer, where they're converted back to function
// calls by WebWorkerProxy.
class WebWorkerClientProxy : public WebWorkerClient,
                             public IPC::Channel::Listener,
                             public base::RefCounted<WebWorkerClientProxy> {
 public:
  WebWorkerClientProxy (const GURL& url, int route_id);

  // WebWorkerClient implementation.
  void PostMessageToWorkerObject(const string16& message);
  void PostExceptionToWorkerObject(
      const string16& error_message,
      int line_number,
      const string16& source_url);
  void PostConsoleMessageToWorkerObject(
      int destination,
      int source,
      int level,
      const string16& message,
      int line_number,
      const string16& source_url);
  void ConfirmMessageFromWorkerObject(bool has_pending_activity);
  void ReportPendingActivity(bool has_pending_activity);
  void WorkerContextDestroyed();

  // IPC::Channel::Listener implementation.
  void OnMessageReceived(const IPC::Message& message);

 private:
  friend class base::RefCounted<WebWorkerClientProxy>;
  ~WebWorkerClientProxy ();

  bool Send(IPC::Message* message);

  // The source url for this worker.
  GURL url_;

  int route_id_;

  WebWorker* impl_;

  DISALLOW_COPY_AND_ASSIGN(WebWorkerClientProxy);
};

#endif  // CHROME_WORKER_WEBWORKERCLIENT_PROXY_H_
