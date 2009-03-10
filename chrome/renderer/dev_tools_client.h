// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_DEV_TOOLS_CLIENT_H_
#define CHROME_RENDERER_DEV_TOOLS_CLIENT_H_

#include "base/basictypes.h"

namespace IPC {
class Message;
}
class MessageLoop;
class RenderView;

// Developer tools UI end of communication channel between the render process of
// the page being inspected and tools UI renderer process. All messages will
// go through browser process. On the side of the inspected page there's
// corresponding DevToolsAgent object.
// TODO(yurys): now the client is almost empty later it will delegate calls to
// code in glue
class DevToolsClient {
 public:
  explicit DevToolsClient(RenderView* view);
  virtual ~DevToolsClient();

  // Called to possibly handle the incoming IPC message. Returns true if
  // handled. Called in render thread.
  bool OnMessageReceived(const IPC::Message& message);

 private:
  void DidDebugAttach();

  // Sends message to DevToolsAgent.
  void Send(const IPC::Message& tools_agent_message);

  RenderView* render_view_;  // host render view

  DISALLOW_COPY_AND_ASSIGN(DevToolsClient);
};

#endif  // CHROME_RENDERER_DEV_TOOLS_CLIENT_H_
