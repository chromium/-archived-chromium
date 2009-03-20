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
#undef LOG

#include "base/json_reader.h"
#include "base/json_writer.h"
#include "base/string_util.h"
#include "base/values.h"

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
      last_call_id_(1) {
  dom_agent_stub_.set(new DomAgentStub(this));
  net_agent_stub_.set(new NetAgentStub(this));
  tools_agent_stub_.set(new ToolsAgentStub(this));

  BindToJavascript(web_view_impl_->GetMainFrame(), L"DevToolsHost");
  BindMethod("getDocumentElement",
      &WebDevToolsClientImpl::JsGetDocumentElement);
  BindMethod("getChildNodes", &WebDevToolsClientImpl::JsGetChildNodes);
  BindMethod("setAttribute", &WebDevToolsClientImpl::JsSetAttribute);
  BindMethod("removeAttribute", &WebDevToolsClientImpl::JsRemoveAttribute);
  BindMethod("setTextNodeValue", &WebDevToolsClientImpl::JsSetTextNodeValue);
  BindMethod("highlightDOMNode", &WebDevToolsClientImpl::JsHighlightDOMNode);
  BindMethod("hideDOMNodeHighlight",
      &WebDevToolsClientImpl::JsHideDOMNodeHighlight);
}

WebDevToolsClientImpl::~WebDevToolsClientImpl() {
}

// DomAgent::DomAgentDelegate implementation.
void WebDevToolsClientImpl::GetDocumentElementResult(
    int call_id,
    const std::string& root) {
  ProcessCallback(call_id, root);
}

void WebDevToolsClientImpl::GetChildNodesResult(
    int call_id,
    const std::string& list) {
  ProcessCallback(call_id, list);
}

void WebDevToolsClientImpl::AttributesUpdated(int id, const Value& attributes) {
  MakeJsCall("dom.setAttributes", id, &attributes);
}

void WebDevToolsClientImpl::ChildNodesUpdated(int id, const Value& children) {
  MakeJsCall("dom.setChildren", id, &children);
}

void WebDevToolsClientImpl::ChildNodeInserted(
    int parent_id,
    int prev_id,
    const Value& value) {
  MakeJsCall("dom.nodeInserted", parent_id, prev_id, &value);
}

void WebDevToolsClientImpl::ChildNodeRemoved(int parent_id, int id) {
  MakeJsCall("dom.nodeRemoved", parent_id, id);
}

void WebDevToolsClientImpl::HasChildrenUpdated(int id, bool new_value) {
  MakeJsCall("dom.setHasChildren", id, true);
}

// NetAgent::NetAgentDelegate implementation.
void WebDevToolsClientImpl::WillSendRequest(
    int identifier,
    const Value& request) {
  MakeJsCall("net.willSendRequest", identifier, &request);
}

void WebDevToolsClientImpl::DidReceiveResponse(
    int identifier,
    const Value& response) {
  MakeJsCall("net.didReceiveResponse", identifier, &response);
}

void WebDevToolsClientImpl::DidFinishLoading(int identifier,
    const Value& response) {
  MakeJsCall("net.didFinishLoading", identifier, &response);
}

void WebDevToolsClientImpl::DidFailLoading(int identifier,
    const Value& response) {
  MakeJsCall("net.didFailLoading", identifier, &response);
}

void WebDevToolsClientImpl::GetResourceContentResult(
    int call_id,
    const std::string& content) {
  ProcessCallback(call_id, content);
}

void WebDevToolsClientImpl::UpdateFocusedNode(int node_id) {
  MakeJsCall("tools.updateFocusedNode", node_id);
}

void WebDevToolsClientImpl::ProcessCallback(
    int call_id,
    const std::string& data) {
  HashMap<int, CppVariant>::iterator it = callbacks_.find(call_id);
  if (it != callbacks_.end()) {
    CppVariant result;
    CppVariant args[2];
    args[0].Set(*GetAsCppVariant());
    args[1].Set(data);
    it->second.Invoke("call", args, 2, result);
    callbacks_.remove(call_id);
  }
}

void WebDevToolsClientImpl::JsGetResourceSource(
    const CppArgumentList& args,
    CppVariant* result) {
  int call_id = last_call_id_++;
  net_agent_stub_->GetResourceContent(call_id, args[0].ToInt32(),
      webkit_glue::StdStringToString(args[1].ToString()));
  callbacks_.set(call_id, args[1]);
}

void WebDevToolsClientImpl::JsGetDocumentElement(
    const CppArgumentList& args,
    CppVariant* result) {
  tools_agent_stub_->SetDomAgentEnabled(true);
  tools_agent_stub_->SetNetAgentEnabled(true);
  int call_id = last_call_id_++;
  dom_agent_stub_->GetDocumentElement(call_id);
  callbacks_.set(call_id, args[0]);
  result->SetNull();
}

void WebDevToolsClientImpl::JsGetChildNodes(
    const CppArgumentList& args,
    CppVariant* result) {
  int call_id = last_call_id_++;
  dom_agent_stub_->GetChildNodes(call_id, args[0].ToInt32());
  callbacks_.set(call_id, args[1]);
  result->SetNull();
}

void WebDevToolsClientImpl::JsSetAttribute(
    const CppArgumentList& args,
    CppVariant* result) {
  dom_agent_stub_->SetAttribute(args[0].ToInt32(),
      webkit_glue::StdStringToString(args[1].ToString()),
      webkit_glue::StdStringToString(args[2].ToString()));
  result->SetNull();
}

void WebDevToolsClientImpl::JsRemoveAttribute(
    const CppArgumentList& args,
    CppVariant* result) {
  dom_agent_stub_->RemoveAttribute(args[0].ToInt32(),
      webkit_glue::StdStringToString(args[1].ToString()));
  result->SetNull();
}

void WebDevToolsClientImpl::JsSetTextNodeValue(
    const CppArgumentList& args,
    CppVariant* result) {
  dom_agent_stub_->SetTextNodeValue(args[0].ToInt32(),
      webkit_glue::StdStringToString(args[1].ToString()));
  result->SetNull();
}

void WebDevToolsClientImpl::JsHighlightDOMNode(
    const CppArgumentList& args,
    CppVariant* result) {
  tools_agent_stub_->HighlightDOMNode(args[0].ToInt32());
  result->SetNull();
}

void WebDevToolsClientImpl::JsHideDOMNodeHighlight(const CppArgumentList& args,
    CppVariant* result) {
  tools_agent_stub_->HideDOMNodeHighlight();
  result->SetNull();
}

void WebDevToolsClientImpl::EvaluateJs(const std::string& expr) {
  web_view_impl_->GetMainFrame()->ExecuteScript(expr);
}

void WebDevToolsClientImpl::DispatchMessageFromAgent(
    const std::string& raw_msg) {
  OwnPtr<ListValue> message(
      static_cast<ListValue*>(DevToolsRpc::ParseMessage(raw_msg)));
  if (DomAgentDelegateDispatch::Dispatch(this, *message.get()))
    return;
  if (NetAgentDelegateDispatch::Dispatch(this, *message.get()))
    return;
  if (ToolsAgentDelegateDispatch::Dispatch(this, *message.get()))
    return;
}

void WebDevToolsClientImpl::SendRpcMessage(const std::string& raw_msg) {
  delegate_->SendMessageToAgent(raw_msg);
}

// static
std::string WebDevToolsClientImpl::ToJSON(const String& value) {
  StringValue str(webkit_glue::StringToStdString(value));
  return ToJSON(&str);
}

// static
std::string WebDevToolsClientImpl::ToJSON(int value) {
  FundamentalValue fund(value);
  return ToJSON(&fund);
}

// static
std::string WebDevToolsClientImpl::ToJSON(const Value* value) {
  std::string json;
  JSONWriter::Write(value, false, &json);
  return json;
}
