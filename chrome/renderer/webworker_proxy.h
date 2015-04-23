// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_WEBWORKER_PROXY_H_
#define CHROME_RENDERER_WEBWORKER_PROXY_H_

#include <vector>

#include "base/basictypes.h"
#include "chrome/common/ipc_channel.h"
#include "webkit/api/public/WebWorker.h"

class ChildThread;
class GURL;
class RenderView;

// This class provides an implementation of WebWorker that the renderer provides
// to the glue.  This class converts function calls to IPC messages that are
// dispatched in the worker process by WebWorkerClientProxy.  It also receives
// IPC messages from WebWorkerClientProxy which it converts to function calls to
// WebWorkerClient.
class WebWorkerProxy : public WebKit::WebWorker,
                       public IPC::Channel::Listener {
 public:
  WebWorkerProxy(WebKit::WebWorkerClient* client,
                 ChildThread* child_thread,
                 int render_view_route_id);
  virtual ~WebWorkerProxy();

  // WebWorker implementation.
  virtual void startWorkerContext(const WebKit::WebURL& script_url,
                                  const WebKit::WebString& user_agent,
                                  const WebKit::WebString& source_code);
  virtual void terminateWorkerContext();
  virtual void postMessageToWorkerContext(const WebKit::WebString& message);
  virtual void workerObjectDestroyed();

  // IPC::Channel::Listener implementation.
  void OnMessageReceived(const IPC::Message& message);

 private:
  bool Send(IPC::Message* message);

  void OnDedicatedWorkerCreated();
  void Disconnect();

  // The routing id used to reach WebWorkerClientProxy in the worker process.
  int route_id_;

  ChildThread* child_thread_;

  // The routing id for the RenderView that created this worker.
  int render_view_route_id_;

  // Used to communicate to the WebCore::Worker object in response to IPC
  // messages.
  WebKit::WebWorkerClient* client_;

  // Stores messages that were sent before the StartWorkerContext message.
  std::vector<IPC::Message*> queued_messages_;

  DISALLOW_COPY_AND_ASSIGN(WebWorkerProxy);
};

#endif  // CHROME_RENDERER_WEBWORKER_PROXY_H_
