// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_WEBDEVTOOLSCLIENT_IMPL_H_
#define WEBKIT_GLUE_WEBDEVTOOLSCLIENT_IMPL_H_

#include <string>

#include "base/string_util.h"
#include "webkit/glue/cpp_bound_class.h"
#include "webkit/glue/devtools/devtools_rpc.h"
#include "webkit/glue/devtools/dom_agent.h"
#include "webkit/glue/devtools/net_agent.h"
#include "webkit/glue/devtools/tools_agent.h"
#include "webkit/glue/webdevtoolsclient.h"

namespace WebCore {
class String;
}

class DomAgentStub;
class NetAgentStub;
class ToolsAgentStub;
class WebDevToolsClientDelegate;
class WebViewImpl;

class WebDevToolsClientImpl : public WebDevToolsClient,
                              public CppBoundClass,
                              public DevToolsRpc::Delegate,
                              public DomAgentDelegate,
                              public NetAgentDelegate,
                              public ToolsAgentDelegate {
 public:
  WebDevToolsClientImpl(
      WebViewImpl* web_view_impl,
      WebDevToolsClientDelegate* delegate);
  virtual ~WebDevToolsClientImpl();

  // DomAgentDelegate implementation.
  virtual void DocumentElementUpdated(const Value& value);
  virtual void AttributesUpdated(int id, const Value& attributes);
  virtual void ChildNodesUpdated(int id, const Value& value);
  virtual void ChildNodeInserted(
      int parent_id,
      int prev_id,
      const Value& value);
  virtual void ChildNodeRemoved(int parent_id, int id);
  virtual void HasChildrenUpdated(int id, bool new_value);

  // NetAgentDelegate implementation.
  virtual void WillSendRequest(
      int identifier,
      const Value& request);
  virtual void DidReceiveResponse(
      int identifier,
      const Value& response);
  virtual void DidFinishLoading(int identifier, const Value& response);
  virtual void DidFailLoading(int identifier, const Value& response);
  virtual void SetResourceContent(
      int identifier,
      const WebCore::String& content);

  // ToolsAgentDelegate implementation.
  virtual void UpdateFocusedNode(int node_id);

  // DevToolsRpc::Delegate implementation.
  virtual void SendRpcMessage(const std::string& raw_msg);

  // WebDevToolsClient implementation.
  virtual void DispatchMessageFromAgent(const std::string& raw_msg);

 private:
  // MakeJsCall templates.
  void MakeJsCall(const std::string& func) {
    EvaluateJs(StringPrintf("%s()", func.c_str()));
  }
  template<class T1>
  void MakeJsCall(const std::string& func, T1 t1) {
    EvaluateJs(StringPrintf("%s(%s)", func.c_str(),
        ToJSON(t1).c_str()));
  }
  template<class T1, class T2>
  void MakeJsCall(const std::string& func, T1 t1, T2 t2) {
    EvaluateJs(StringPrintf("%s(%s, %s)", func.c_str(),
        ToJSON(t1).c_str(), ToJSON(t2).c_str()));
  }
  template<class T1, class T2, class T3>
  void MakeJsCall(const std::string& func, T1 t1, T2 t2, T3 t3) {
    EvaluateJs(StringPrintf("%s(%s, %s, %s)", func.c_str(),
        ToJSON(t1).c_str(), ToJSON(t2).c_str(), ToJSON(t3).c_str()));
  }
  template<class T1, class T2, class T3, class T4>
  void MakeJsCall(const std::string& func, T1 t1, T2 t2, T3 t3, T4 t4) {
    EvaluateJs(StringPrintf("%s(%s, %s, %s, %s)", func.c_str(),
        ToJSON(t1).c_str(), ToJSON(t2).c_str(), ToJSON(t3).c_str(),
        ToJSON(t4).c_str()));
  }
  template<class T1, class T2, class T3, class T4, class T5>
  void MakeJsCall(const std::string& func, T1 t1, T2 t2, T3 t3, T4 t4,
      T5 t5) {
    EvaluateJs(StringPrintf("%s(%s, %s, %s, %s, %s)",
        func.c_str(), ToJSON(t1).c_str(), ToJSON(t2).c_str(),
        ToJSON(t3).c_str(), ToJSON(t4).c_str(), ToJSON(t5).c_str()));
  }

  void EvaluateJs(const std::string& expr);

  void JsGetResourceSource(const CppArgumentList& args, CppVariant* result);

  void JsGetDocumentElement(const CppArgumentList& args, CppVariant* result);

  void JsGetChildNodes(const CppArgumentList& args, CppVariant* result);

  void JsSetAttribute(const CppArgumentList& args, CppVariant* result);

  void JsRemoveAttribute(const CppArgumentList& args, CppVariant* result);

  void JsSetTextNodeValue(const CppArgumentList& args, CppVariant* result);

  void JsHighlightDOMNode(const CppArgumentList& args, CppVariant* result);

  void JsHideDOMNodeHighlight(const CppArgumentList& args, CppVariant* result);

 private:
  // Serializers
  static std::string ToJSON(const WebCore::String& value);
  static std::string ToJSON(int value);
  static std::string ToJSON(const Value* value);

  WebViewImpl* web_view_impl_;
  WebDevToolsClientDelegate* delegate_;
  scoped_ptr<DomAgentStub> dom_agent_stub_;
  scoped_ptr<NetAgentStub> net_agent_stub_;
  scoped_ptr<ToolsAgentStub> tools_agent_stub_;
  DomAgentDelegateDispatch dom_agent_delegate_dispatch_;
  NetAgentDelegateDispatch net_agent_delegate_dispatch_;
  ToolsAgentDelegateDispatch tools_agent_delegate_dispatch_;
  DISALLOW_COPY_AND_ASSIGN(WebDevToolsClientImpl);
};

#endif  // WEBKIT_GLUE_WEBDEVTOOLSCLIENT_IMPL_H_
