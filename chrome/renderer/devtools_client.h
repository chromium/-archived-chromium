// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_DEVTOOLS_CLIENT_H_
#define CHROME_RENDERER_DEVTOOLS_CLIENT_H_

#include <string>

#include "base/scoped_ptr.h"
#include "webkit/glue/webdevtoolsclient_delegate.h"

namespace IPC {
class Message;
}
class MessageLoop;
class RenderView;
class WebDevToolsClient;

// Developer tools UI end of communication channel between the render process of
// the page being inspected and tools UI renderer process. All messages will
// go through browser process. On the side of the inspected page there's
// corresponding DevToolsAgent object.
// TODO(yurys): now the client is almost empty later it will delegate calls to
// code in glue
class DevToolsClient : public WebDevToolsClientDelegate {
 public:
  explicit DevToolsClient(RenderView* view);
  virtual ~DevToolsClient();

  // Called to possibly handle the incoming IPC message. Returns true if
  // handled. Called in render thread.
  bool OnMessageReceived(const IPC::Message& message);

  // WebDevToolsClient::Delegate implementation
  virtual void SendMessageToAgent(const std::string& raw_msg);
  virtual void SendDebuggerCommandToAgent(const std::string& command);

 private:
  void OnRpcMessage(const std::string& raw_msg);

  // Sends message to DevToolsAgent.
  void Send(const IPC::Message& tools_agent_message);

  RenderView* render_view_;  // host render view
  scoped_ptr<WebDevToolsClient> web_tools_client_;

  DISALLOW_COPY_AND_ASSIGN(DevToolsClient);
};

#endif  // CHROME_RENDERER_DEVTOOLS_CLIENT_H_
