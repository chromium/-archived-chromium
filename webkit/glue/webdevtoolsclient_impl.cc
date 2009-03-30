// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include <string>

#include "Document.h"
#include "InspectorController.h"
#include "Node.h"
#include "Page.h"
#include "PlatformString.h"
#include <wtf/OwnPtr.h>
#include <wtf/Vector.h>
#undef LOG

#include "base/json_reader.h"
#include "base/json_writer.h"
#include "base/string_util.h"
#include "base/values.h"

#include "webkit/glue/devtools/debugger_agent.h"
#include "webkit/glue/devtools/devtools_rpc_js.h"
#include "webkit/glue/devtools/dom_agent.h"
#include "webkit/glue/devtools/net_agent.h"
#include "webkit/glue/devtools/tools_agent.h"
#include "webkit/glue/glue_util.h"
#include "webkit/glue/webdevtoolsclient_delegate.h"
#include "webkit/glue/webdevtoolsclient_impl.h"
#include "webkit/glue/webframe.h"
#include "webkit/glue/webview_impl.h"

using WebCore::CString;
using WebCore::Document;
using WebCore::InspectorController;
using WebCore::Node;
using WebCore::Page;
using WebCore::String;

DEFINE_RPC_JS_BOUND_OBJ(DebuggerAgent, DEBUGGER_AGENT_STRUCT,
    DebuggerAgentDelegate, DEBUGGER_AGENT_DELEGATE_STRUCT)
DEFINE_RPC_JS_BOUND_OBJ(DomAgent, DOM_AGENT_STRUCT,
    DomAgentDelegate, DOM_AGENT_DELEGATE_STRUCT)
DEFINE_RPC_JS_BOUND_OBJ(NetAgent, NET_AGENT_STRUCT,
    NetAgentDelegate, NET_AGENT_DELEGATE_STRUCT)
DEFINE_RPC_JS_BOUND_OBJ(ToolsAgent, TOOLS_AGENT_STRUCT,
    ToolsAgentDelegate, TOOLS_AGENT_DELEGATE_STRUCT)

namespace {

class RemoteDebuggerCommandExecutor : public CppBoundClass {
 public:
  RemoteDebuggerCommandExecutor(
      WebDevToolsClientDelegate* delegate,
      WebFrame* frame,
      const std::wstring& classname)
      : delegate_(delegate) {
    BindToJavascript(frame, classname);
    BindMethod("DebuggerCommand",
                &RemoteDebuggerCommandExecutor::DebuggerCommand);
  }
  virtual ~RemoteDebuggerCommandExecutor() {}

  // The DebuggerCommand() function provided to Javascript.
  void DebuggerCommand(const CppArgumentList& args, CppVariant* result) {
    std::string command = args[0].ToString();
    result->SetNull();
    delegate_->SendDebuggerCommandToAgent(command);
  }

 private:
  WebDevToolsClientDelegate* delegate_;
  DISALLOW_COPY_AND_ASSIGN(RemoteDebuggerCommandExecutor);
};

} //  namespace

/*static*/
WebDevToolsClient* WebDevToolsClient::Create(
    WebView* view,
    WebDevToolsClientDelegate* delegate) {
  return new WebDevToolsClientImpl(static_cast<WebViewImpl*>(view), delegate);
}

WebDevToolsClientImpl::WebDevToolsClientImpl(
    WebViewImpl* web_view_impl,
    WebDevToolsClientDelegate* delegate)
    : web_view_impl_(web_view_impl),
      delegate_(delegate),
      loaded_(false) {
  WebFrame* frame = web_view_impl_->GetMainFrame();

  // Debugger commands should be sent using special method.
  debugger_command_executor_obj_.set(new RemoteDebuggerCommandExecutor(
      delegate, frame, L"RemoteDebuggerCommandExecutor"));
  debugger_agent_obj_.set(new JsDebuggerAgentBoundObj(
      this, frame, L"RemoteDebuggerAgent"));
  dom_agent_obj_.set(new JsDomAgentBoundObj(this, frame, L"RemoteDomAgent"));
  net_agent_obj_.set(new JsNetAgentBoundObj(this, frame, L"RemoteNetAgent"));
  tools_agent_obj_.set(
      new JsToolsAgentBoundObj(this, frame, L"RemoteToolsAgent"));

  BindToJavascript(frame, L"DevToolsHost");
  BindMethod("addSourceToFrame", &WebDevToolsClientImpl::JsAddSourceToFrame);
  BindMethod("loaded", &WebDevToolsClientImpl::JsLoaded);
}

WebDevToolsClientImpl::~WebDevToolsClientImpl() {
}

void WebDevToolsClientImpl::DispatchMessageFromAgent(
    const std::string& raw_msg) {
  if (!loaded_) {
    pending_incoming_messages_.append(raw_msg);
    return;
  }
  OwnPtr<ListValue> message(
      static_cast<ListValue*>(DevToolsRpc::ParseMessage(raw_msg)));

  std::string expr;
  if (dom_agent_obj_->Dispatch(*message.get(), &expr)
          || net_agent_obj_->Dispatch(*message.get(), &expr)
          || tools_agent_obj_->Dispatch(*message.get(), &expr)
          || debugger_agent_obj_->Dispatch(*message.get(), &expr)) {
    web_view_impl_->GetMainFrame()->ExecuteScript(expr);
  }
}

void WebDevToolsClientImpl::SendRpcMessage(const std::string& raw_msg) {
  delegate_->SendMessageToAgent(raw_msg);
}

void WebDevToolsClientImpl::JsAddSourceToFrame(
    const CppArgumentList& args,
    CppVariant* result) {
  std::string mime_type = args[0].ToString();
  std::string source = args[1].ToString();
  std::string node_id = args[2].ToString();

  Page* page = web_view_impl_->page();
  Document* document = page->mainFrame()->document();
  Node* node = document->getElementById(
      webkit_glue::StdStringToString(node_id));
  if (!node) {
    result->Set(false);
    return;
  }

  bool r = page->inspectorController()->addSourceToFrame(
      webkit_glue::StdStringToString(mime_type),
      webkit_glue::StdStringToString(source),
      node);
  result->Set(r);
}

void WebDevToolsClientImpl::JsLoaded(
    const CppArgumentList& args,
    CppVariant* result) {
  loaded_ = true;
  for (Vector<std::string>::iterator it = pending_incoming_messages_.begin();
       it != pending_incoming_messages_.end(); ++it) {
    DispatchMessageFromAgent(*it);
  }
  pending_incoming_messages_.clear();
  result->SetNull();
}
