// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_WEBDEVTOOLSAGENT_IMPL_H_
#define WEBKIT_GLUE_WEBDEVTOOLSAGENT_IMPL_H_

#include <string>

#include "base/scoped_ptr.h"
#include "webkit/glue/devtools/devtools_rpc.h"
#include "webkit/glue/devtools/dom_agent.h"
#include "webkit/glue/devtools/net_agent.h"
#include "webkit/glue/devtools/tools_agent.h"
#include "webkit/glue/webdevtoolsagent.h"

namespace WebCore {
class Document;
class Node;
class String;
}

class DomAgentImpl;
class NetAgentImpl;
class Value;
class WebDevToolsAgentDelegate;
class WebViewImpl;

class WebDevToolsAgentImpl
    : public WebDevToolsAgent,
      public ToolsAgent,
      public DevToolsRpc::Delegate {
 public:
  WebDevToolsAgentImpl(WebViewImpl* web_view_impl,
      WebDevToolsAgentDelegate* delegate);
  virtual ~WebDevToolsAgentImpl();

  // ToolsAgent implementation.
  virtual void SetDomAgentEnabled(bool enabled);
  virtual void SetNetAgentEnabled(bool enabled);
  virtual void HighlightDOMNode(int node_id);
  virtual void HideDOMNodeHighlight();

  // WebDevToolsAgent implementation.
  virtual void DispatchMessageFromClient(const std::string& raw_msg);

  // DevToolsRpc::Delegate implementation.
  void SendRpcMessage(const std::string& raw_msg);

  // Methods called by the glue.
  void SetMainFrameDocumentReady(bool ready);
  void Inspect(WebCore::Node* node);

  DomAgentImpl* dom_agent_impl() { return dom_agent_impl_.get(); }
  NetAgentImpl* net_agent_impl() { return net_agent_impl_.get(); }
  WebViewImpl* web_view_impl() { return web_view_impl_; }

 private:
  WebDevToolsAgentDelegate* delegate_;
  WebViewImpl* web_view_impl_;
  WebCore::Document* document_;
  scoped_ptr<DomAgentImpl> dom_agent_impl_;
  scoped_ptr<NetAgentImpl> net_agent_impl_;
  scoped_ptr<DomAgentDelegateStub> dom_agent_delegate_stub_;
  scoped_ptr<NetAgentDelegateStub> net_agent_delegate_stub_;
  scoped_ptr<ToolsAgentDelegateStub> tools_agent_delegate_stub_;
  DomAgentDispatch dom_agent_dispatch_;
  NetAgentDispatch net_agent_dispatch_;
  ToolsAgentDispatch tools_agent_dispatch_;
  DISALLOW_COPY_AND_ASSIGN(WebDevToolsAgentImpl);
};

#endif  // WEBKIT_GLUE_WEBDEVTOOLSAGENT_IMPL_H_
