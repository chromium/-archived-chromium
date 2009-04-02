// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include <string>

#include "Document.h"
#include "EventListener.h"
#include "InspectorController.h"
#include "Node.h"
#include "Page.h"
#include "PlatformString.h"
#include "ScriptValue.h"
#include <wtf/OwnPtr.h>
#undef LOG

#include "base/values.h"
#include "webkit/glue/devtools/debugger_agent_impl.h"
#include "webkit/glue/devtools/debugger_agent_manager.h"
#include "webkit/glue/devtools/dom_agent_impl.h"
#include "webkit/glue/devtools/net_agent_impl.h"
#include "webkit/glue/glue_util.h"
#include "webkit/glue/webdatasource.h"
#include "webkit/glue/webdevtoolsagent_delegate.h"
#include "webkit/glue/webdevtoolsagent_impl.h"
#include "webkit/glue/weburlrequest.h"
#include "webkit/glue/webview_impl.h"

using WebCore::Document;
using WebCore::InspectorController;
using WebCore::Node;
using WebCore::Page;
using WebCore::ScriptValue;
using WebCore::String;

WebDevToolsAgentImpl::WebDevToolsAgentImpl(
    WebViewImpl* web_view_impl,
    WebDevToolsAgentDelegate* delegate)
    : delegate_(delegate),
      web_view_impl_(web_view_impl),
      document_(NULL),
      attached_(false) {
  debugger_agent_delegate_stub_.set(new DebuggerAgentDelegateStub(this));
  dom_agent_delegate_stub_.set(new DomAgentDelegateStub(this));
  net_agent_delegate_stub_.set(new NetAgentDelegateStub(this));
  tools_agent_delegate_stub_.set(new ToolsAgentDelegateStub(this));
}

WebDevToolsAgentImpl::~WebDevToolsAgentImpl() {
}

void WebDevToolsAgentImpl::Attach() {
  if (attached_) {
    return;
  }
  debugger_agent_impl_.set(
      new DebuggerAgentImpl(debugger_agent_delegate_stub_.get()));
  dom_agent_impl_.set(new DomAgentImpl(dom_agent_delegate_stub_.get()));
  net_agent_impl_.set(new NetAgentImpl(net_agent_delegate_stub_.get()));
  if (document_) {
    debugger_agent_impl_->SetDocument(document_);
    dom_agent_impl_->SetDocument(document_);
    net_agent_impl_->SetDocument(document_);
  }
  attached_ = true;
}

void WebDevToolsAgentImpl::Detach() {
  debugger_agent_impl_.set(NULL);
  dom_agent_impl_.set(NULL);
  net_agent_impl_.set(NULL);
  attached_ = false;
}

void WebDevToolsAgentImpl::SetMainFrameDocumentReady(bool ready) {
  // Store document reference no matter if client is attached.
  if (ready) {
    Page* page = web_view_impl_->page();
    document_ = page->mainFrame()->document();
  } else {
    document_ = NULL;
  }
  if (attached_) {
    debugger_agent_impl_->SetDocument(document_);
    dom_agent_impl_->SetDocument(document_);
    net_agent_impl_->SetDocument(document_);
  }
}

void WebDevToolsAgentImpl::DidCommitLoadForFrame(
    WebViewImpl* webview,
    WebFrame* frame,
    bool is_new_navigation) {
  if (!attached_) {
    return;
  }
  WebDataSource* ds = frame->GetDataSource();
  const WebRequest& request = ds->GetRequest();
  GURL url = ds->HasUnreachableURL() ?
      ds->GetUnreachableURL() :
      request.GetURL();
  tools_agent_delegate_stub_->FrameNavigate(
      url.possibly_invalid_spec(),
      webview->GetMainFrame() == frame);
}

void WebDevToolsAgentImpl::HighlightDOMNode(int node_id) {
  if (!attached_) {
    return;
  }
  Node* node = dom_agent_impl_->GetNodeForId(node_id);
  if (!node) {
    return;
  }
  Page* page = web_view_impl_->page();
  page->inspectorController()->highlight(node);
}

void WebDevToolsAgentImpl::HideDOMNodeHighlight() {
  Page* page = web_view_impl_->page();
  page->inspectorController()->hideHighlight();
}

void WebDevToolsAgentImpl::EvaluateJavaScript(int call_id, const String& js) {
  Page* page = web_view_impl_->page();
  if (!page->mainFrame()) {
    return;
  }
  ScriptValue result = page->mainFrame()->loader()->executeScript(js);
  String result_string;
  if (!result.hasNoValue()) {
    result_string = result.toString(NULL);
  }
  tools_agent_delegate_stub_->DidEvaluateJavaScript(call_id, result_string);
}

void WebDevToolsAgentImpl::ExecuteUtilityFunction(
      int call_id,
      const String& function_name,
      int node_id,
      const String& json_args) {
  Node* node = dom_agent_impl_->GetNodeForId(node_id);
  String result;
  if (node) {
    result = debugger_agent_impl_->ExecuteUtilityFunction(function_name, node,
        json_args);
  }
  tools_agent_delegate_stub_->DidExecuteUtilityFunction(call_id,
      result);
}

void WebDevToolsAgentImpl::DispatchMessageFromClient(
    const std::string& raw_msg) {
  OwnPtr<ListValue> message(
      static_cast<ListValue*>(DevToolsRpc::ParseMessage(raw_msg)));
  if (ToolsAgentDispatch::Dispatch(this, *message.get())) {
    return;
  }

  if (!attached_) {
    return;
  }

  if (debugger_agent_impl_.get() &&
      DebuggerAgentDispatch::Dispatch(
          debugger_agent_impl_.get(),
          *message.get())) {
    return;
  }

  if (DomAgentDispatch::Dispatch(dom_agent_impl_.get(), *message.get())) {
    return;
  }
  if (NetAgentDispatch::Dispatch(net_agent_impl_.get(), *message.get())) {
    return;
  }
}

void WebDevToolsAgentImpl::InspectElement(int x, int y) {
  Node* node = web_view_impl_->GetNodeForWindowPos(x, y);
  if (!node) {
    return;
  }

  int node_id = dom_agent_impl_->PushNodePathToClient(node);
  tools_agent_delegate_stub_->UpdateFocusedNode(node_id);
}

void WebDevToolsAgentImpl::SendRpcMessage(const std::string& raw_msg) {
  delegate_->SendMessageToClient(raw_msg);
}

// static
void WebDevToolsAgent::ExecuteDebuggerCommand(const std::string& command) {
  DebuggerAgentManager::ExecuteDebuggerCommand(command);
}
