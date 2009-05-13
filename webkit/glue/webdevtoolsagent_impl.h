// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_WEBDEVTOOLSAGENT_IMPL_H_
#define WEBKIT_GLUE_WEBDEVTOOLSAGENT_IMPL_H_

#include <string>

#include <wtf/OwnPtr.h>
#include <wtf/Vector.h>

#include "v8.h"
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

class DebuggerAgentDelegateStub;
class DebuggerAgentImpl;
class DomAgentImpl;
class NetAgentImpl;
class Value;
class WebDevToolsAgentDelegate;
class WebFrame;
class WebFrameImpl;
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
  virtual void HighlightDOMNode(int node_id);
  virtual void HideDOMNodeHighlight();
  virtual void EvaluateJavaScript(int call_id, const String& js);
  virtual void ExecuteUtilityFunction(
      int call_id,
      const WebCore::String& function_name,
      int node_id,
      const WebCore::String& json_args);
  virtual void ClearConsoleMessages();

  // WebDevToolsAgent implementation.
  virtual void Attach();
  virtual void Detach();
  virtual void DispatchMessageFromClient(const std::string& raw_msg);
  virtual void InspectElement(int x, int y);

  // DevToolsRpc::Delegate implementation.
  void SendRpcMessage(const std::string& raw_msg);

  // Methods called by the glue.
  void SetMainFrameDocumentReady(bool ready);
  void DidCommitLoadForFrame(WebViewImpl* webview,
                             WebFrame* frame,
                             bool is_new_navigation);
  void AddMessageToConsole(
      int source,
      int level,
      const WebCore::String& message,
      unsigned int line_no,
      const WebCore::String& source_id);

  void WindowObjectCleared(WebFrameImpl* webframe);

  void ForceRepaint();

  int host_id() { return host_id_; }
  NetAgentImpl* net_agent_impl() { return net_agent_impl_.get(); }

 private:
  struct ConsoleMessage {
    ConsoleMessage(
        int src, int lvl, const String& m, unsigned li, const String& sid)
        : source(src),
          level(lvl),
          text(m),
          line_no(li),
          source_id(sid) {
    }
    int source;
    int level;
    WebCore::String text;
    WebCore::String source_id;
    unsigned int line_no;
  };

  static void Serialize(const ConsoleMessage& message, DictionaryValue* value);

  int host_id_;
  WebDevToolsAgentDelegate* delegate_;
  WebViewImpl* web_view_impl_;
  WebCore::Document* document_;
  OwnPtr<DebuggerAgentDelegateStub> debugger_agent_delegate_stub_;
  OwnPtr<DomAgentDelegateStub> dom_agent_delegate_stub_;
  OwnPtr<NetAgentDelegateStub> net_agent_delegate_stub_;
  OwnPtr<ToolsAgentDelegateStub> tools_agent_delegate_stub_;
  OwnPtr<DebuggerAgentImpl> debugger_agent_impl_;
  OwnPtr<DomAgentImpl> dom_agent_impl_;
  OwnPtr<NetAgentImpl> net_agent_impl_;
  Vector<ConsoleMessage> console_log_;
  bool attached_;
  // TODO(pfeldman): This should not be needed once GC styles issue is fixed
  // for matching rules.
  v8::Persistent<v8::Context> utility_context_;
  DISALLOW_COPY_AND_ASSIGN(WebDevToolsAgentImpl);
};

#endif  // WEBKIT_GLUE_WEBDEVTOOLSAGENT_IMPL_H_
