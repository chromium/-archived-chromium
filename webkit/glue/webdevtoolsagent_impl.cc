// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include <string>

#include "Document.h"
#include "EventListener.h"
#include "InspectorController.h"
#include "InspectorFrontend.h"
#include "InspectorResource.h"
#include "Node.h"
#include "Page.h"
#include "PlatformString.h"
#include "ScriptObject.h"
#include "ScriptState.h"
#include "ScriptValue.h"
#include "V8Binding.h"
#include "V8Proxy.h"
#include <wtf/OwnPtr.h>
#undef LOG

#include "base/values.h"
#include "webkit/api/public/WebDataSource.h"
#include "webkit/api/public/WebURL.h"
#include "webkit/api/public/WebURLRequest.h"
#include "webkit/glue/devtools/bound_object.h"
#include "webkit/glue/devtools/debugger_agent_impl.h"
#include "webkit/glue/devtools/debugger_agent_manager.h"
#include "webkit/glue/devtools/dom_agent_impl.h"
#include "webkit/glue/glue_util.h"
#include "webkit/glue/webdevtoolsagent_delegate.h"
#include "webkit/glue/webdevtoolsagent_impl.h"
#include "webkit/glue/webview_impl.h"

using WebCore::Document;
using WebCore::InspectorController;
using WebCore::InspectorFrontend;
using WebCore::InspectorResource;
using WebCore::Node;
using WebCore::Page;
using WebCore::ScriptValue;
using WebCore::String;
using WebCore::V8ClassIndex;
using WebCore::V8Proxy;
using WebKit::WebDataSource;
using WebKit::WebURLRequest;

WebDevToolsAgentImpl::WebDevToolsAgentImpl(
    WebViewImpl* web_view_impl,
    WebDevToolsAgentDelegate* delegate)
    : host_id_(delegate->GetHostId()),
      delegate_(delegate),
      web_view_impl_(web_view_impl),
      document_(NULL),
      attached_(false) {
  debugger_agent_delegate_stub_.set(new DebuggerAgentDelegateStub(this));
  dom_agent_delegate_stub_.set(new DomAgentDelegateStub(this));
  tools_agent_delegate_stub_.set(new ToolsAgentDelegateStub(this));
  tools_agent_native_delegate_stub_.set(new ToolsAgentNativeDelegateStub(this));
}

WebDevToolsAgentImpl::~WebDevToolsAgentImpl() {
  DebuggerAgentManager::OnWebViewClosed(web_view_impl_);
  DisposeUtilityContext();
}

void WebDevToolsAgentImpl::DisposeUtilityContext() {
  if (!utility_context_.IsEmpty()) {
    utility_context_.Dispose();
    utility_context_.Clear();
  }
}

void WebDevToolsAgentImpl::Attach() {
  if (attached_) {
    return;
  }
  debugger_agent_impl_.set(
      new DebuggerAgentImpl(web_view_impl_,
                            debugger_agent_delegate_stub_.get(),
                            this));
  dom_agent_impl_.set(new DomAgentImpl(dom_agent_delegate_stub_.get()));

  // We are potentially attaching to the running page -> init agents with
  // Document if any.
  Page* page = web_view_impl_->page();
  Document* doc = page->mainFrame()->document();
  if (doc) {
    // Reuse existing context in case detached/attached.
    if (utility_context_.IsEmpty()) {
      debugger_agent_impl_->ResetUtilityContext(doc, &utility_context_);
      InitDevToolsAgentHost();
    }

    dom_agent_impl_->SetDocument(doc);

    InspectorController* ic = web_view_impl_->page()->inspectorController();
    // Unhide resources panel if necessary.
    tools_agent_delegate_stub_->SetResourcesPanelEnabled(
        ic->resourceTrackingEnabled());
    v8::HandleScope scope;
    ic->setFrontendProxyObject(
        scriptStateFromPage(web_view_impl_->page()),
        utility_context_->Global());
    // Allow controller to send messages to the frontend.
    ic->setWindowVisible(true, false);
  }
  attached_ = true;
}

void WebDevToolsAgentImpl::Detach() {
  // Prevent controller from sending messages to the frontend.
  InspectorController* ic = web_view_impl_->page()->inspectorController();
  ic->setWindowVisible(false, false);
  HideDOMNodeHighlight();
  devtools_agent_host_.set(NULL);
  debugger_agent_impl_.set(NULL);
  dom_agent_impl_.set(NULL);
  attached_ = false;
}

void WebDevToolsAgentImpl::SetMainFrameDocumentReady(bool ready) {
  if (!attached_) {
    return;
  }

  // We were attached prior to the page load -> init agents with Document.
  Document* doc;
  if (ready) {
    Page* page = web_view_impl_->page();
    doc = page->mainFrame()->document();
  } else {
    doc = NULL;
  }
  debugger_agent_impl_->ResetUtilityContext(doc, &utility_context_);
  if (doc) {
    InitDevToolsAgentHost();
  }
  dom_agent_impl_->SetDocument(doc);
}

void WebDevToolsAgentImpl::DidCommitLoadForFrame(
    WebViewImpl* webview,
    WebFrame* frame,
    bool is_new_navigation) {
  if (!attached_) {
    DisposeUtilityContext();
    return;
  }
  WebDataSource* ds = frame->GetDataSource();
  const WebURLRequest& request = ds->request();
  GURL url = ds->hasUnreachableURL() ?
      ds->unreachableURL() :
      request.url();
  tools_agent_delegate_stub_->FrameNavigate(
      url.possibly_invalid_spec(),
      webview->GetMainFrame() == frame);
  InspectorController* ic = webview->page()->inspectorController();
  // Unhide resources panel if necessary.
  tools_agent_delegate_stub_->SetResourcesPanelEnabled(
      ic->resourceTrackingEnabled());
}

void WebDevToolsAgentImpl::WindowObjectCleared(WebFrameImpl* webframe) {
  DebuggerAgentManager::SetHostId(webframe, host_id_);
}

void WebDevToolsAgentImpl::ForceRepaint() {
  delegate_->ForceRepaint();
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
  if (page) {
    page->inspectorController()->hideHighlight();
  }
}

void WebDevToolsAgentImpl::ExecuteUtilityFunction(
      int call_id,
      const String& function_name,
      const String& json_args) {
  String result;
  String exception;
  result = debugger_agent_impl_->ExecuteUtilityFunction(utility_context_,
      function_name, json_args, &exception);
  tools_agent_delegate_stub_->DidExecuteUtilityFunction(call_id,
      result, exception);
}

void WebDevToolsAgentImpl::ClearConsoleMessages() {
  Page* page = web_view_impl_->page();
  if (page) {
    page->inspectorController()->clearConsoleMessages();
  }
}

void WebDevToolsAgentImpl::GetResourceContent(
    int call_id,
    int identifier) {
  String content;
  Page* page = web_view_impl_->page();
  if (page) {
    RefPtr<InspectorResource> resource =
        page->inspectorController()->resources().get(identifier);
    if (resource.get()) {
      content = resource->sourceString();
    }
  }
  tools_agent_native_delegate_stub_->DidGetResourceContent(call_id, content);
}

void WebDevToolsAgentImpl::SetResourceTrackingEnabled(
    bool enabled,
    bool always) {
  // Hide / unhide resources panel if necessary.
  tools_agent_delegate_stub_->SetResourcesPanelEnabled(enabled);

  InspectorController* ic = web_view_impl_->page()->inspectorController();
  if (enabled) {
    ic->enableResourceTracking(always);
  } else {
    ic->disableResourceTracking(always);
  }
}

void WebDevToolsAgentImpl::DispatchMessageFromClient(
    const std::string& class_name,
    const std::string& method_name,
    const std::string& raw_msg) {
  OwnPtr<ListValue> message(
      static_cast<ListValue*>(DevToolsRpc::ParseMessage(raw_msg)));
  if (ToolsAgentDispatch::Dispatch(
      this, class_name, method_name, *message.get())) {
    return;
  }

  if (!attached_) {
    return;
  }

  if (debugger_agent_impl_.get() &&
      DebuggerAgentDispatch::Dispatch(
          debugger_agent_impl_.get(), class_name, method_name,
          *message.get())) {
    return;
  }

  if (DomAgentDispatch::Dispatch(
      dom_agent_impl_.get(), class_name, method_name, *message.get())) {
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

void WebDevToolsAgentImpl::SendRpcMessage(
    const std::string& class_name,
    const std::string& method_name,
    const std::string& raw_msg) {
  delegate_->SendMessageToClient(class_name, method_name, raw_msg);
}

void WebDevToolsAgentImpl::InitDevToolsAgentHost() {
  devtools_agent_host_.set(
      new BoundObject(utility_context_, this, "DevToolsAgentHost"));
  devtools_agent_host_->AddProtoFunction(
      "dispatch",
      WebDevToolsAgentImpl::JsDispatchOnClient);
  devtools_agent_host_->AddProtoFunction(
      "getNodeForId",
      WebDevToolsAgentImpl::JsGetNodeForId);
  devtools_agent_host_->Build();
}

// static
v8::Handle<v8::Value> WebDevToolsAgentImpl::JsDispatchOnClient(
    const v8::Arguments& args) {
  v8::TryCatch exception_catcher;
  String message = WebCore::toWebCoreStringWithNullCheck(args[0]);
  if (message.isEmpty() || exception_catcher.HasCaught()) {
    return v8::Undefined();
  }
  WebDevToolsAgentImpl* agent = static_cast<WebDevToolsAgentImpl*>(
      v8::External::Cast(*args.Data())->Value());
  agent->tools_agent_delegate_stub_->DispatchOnClient(message);
  return v8::Undefined();
}

// static
v8::Handle<v8::Value> WebDevToolsAgentImpl::JsGetNodeForId(
    const v8::Arguments& args) {
  int node_id = static_cast<int>(args[0]->NumberValue());
  WebDevToolsAgentImpl* agent = static_cast<WebDevToolsAgentImpl*>(
      v8::External::Cast(*args.Data())->Value());
  Node* node = agent->dom_agent_impl_->GetNodeForId(node_id);
  return V8Proxy::convertToV8Object(V8ClassIndex::NODE, node);
}

// static
void WebDevToolsAgent::ExecuteDebuggerCommand(
    const std::string& command,
    int caller_id) {
  DebuggerAgentManager::ExecuteDebuggerCommand(command, caller_id);
}

// static
void WebDevToolsAgent::SetMessageLoopDispatchHandler(
    MessageLoopDispatchHandler handler) {
  DebuggerAgentManager::SetMessageLoopDispatchHandler(handler);
}
