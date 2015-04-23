// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_DEVTOOLS_AGENT_H_
#define CHROME_RENDERER_DEVTOOLS_AGENT_H_

#include <map>
#include <string>

#include "webkit/glue/webdevtoolsagent_delegate.h"

namespace IPC {
class Message;
}

class RenderView;
class WebDevToolsAgent;

// DevToolsAgent belongs to the inspectable RenderView and provides Glue's
// agents with the communication capabilities. All messages from/to Glue's
// agents infrastructure are flowing through this comminucation agent.
// There is a corresponding DevToolsClient object on the client side.
class DevToolsAgent : public WebDevToolsAgentDelegate {
 public:
  DevToolsAgent(int routing_id, RenderView* view);
  virtual ~DevToolsAgent();

  // IPC message interceptor. Called on the Render thread.
  virtual bool OnMessageReceived(const IPC::Message& message);

  // WebDevToolsAgentDelegate implementation
  virtual void SendMessageToClient(const std::string& class_name,
                                   const std::string& method_name,
                                   const std::string& raw_msg);
  virtual int GetHostId();
  virtual void ForceRepaint();

  // Returns agent instance for its host id.
  static DevToolsAgent* FromHostId(int host_id);

  RenderView* render_view() { return view_; }

  WebDevToolsAgent* GetWebAgent();

 private:
  friend class DevToolsAgentFilter;

  void OnAttach();
  void OnDetach();
  void OnRpcMessage(const std::string& class_name,
                    const std::string& method_name,
                    const std::string& raw_msg);
  void OnInspectElement(int x, int y);

  static std::map<int, DevToolsAgent*> agent_for_routing_id_;

  int routing_id_; //  View routing id that we can access from IO thread.
  RenderView* view_;

  DISALLOW_COPY_AND_ASSIGN(DevToolsAgent);
};

#endif  // CHROME_RENDERER_DEVTOOLS_AGENT_H_
