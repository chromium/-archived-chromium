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
#include <wtf/OwnPtr.h>
#undef LOG

#include "base/values.h"
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
using WebCore::String;

WebDevToolsAgentImpl::WebDevToolsAgentImpl(
    WebViewImpl* web_view_impl,
    WebDevToolsAgentDelegate* delegate)
    : delegate_(delegate),
      web_view_impl_(web_view_impl),
      document_(NULL),
      enabled_(false) {
  dom_agent_delegate_stub_.reset(new DomAgentDelegateStub(this));
  net_agent_delegate_stub_.reset(new NetAgentDelegateStub(this));
  tools_agent_delegate_stub_.reset(new ToolsAgentDelegateStub(this));
}

WebDevToolsAgentImpl::~WebDevToolsAgentImpl() {
}

void WebDevToolsAgentImpl::SetEnabled(bool enabled) {
  if (enabled && !enabled_) {
    dom_agent_impl_.reset(new DomAgentImpl(dom_agent_delegate_stub_.get()));
    net_agent_impl_.reset(new NetAgentImpl(net_agent_delegate_stub_.get()));
    if (document_) {
      dom_agent_impl_->SetDocument(document_);
      net_agent_impl_->SetDocument(document_);
    }
    enabled_ = true;
  } else if (!enabled) {
    dom_agent_impl_.reset(NULL);
    net_agent_impl_.reset(NULL);
    enabled_ = false;
  }
}

void WebDevToolsAgentImpl::SetMainFrameDocumentReady(bool ready) {
  if (ready) {
    Page* page = web_view_impl_->page();
    document_ = page->mainFrame()->document();
  } else {
    document_ = NULL;
  }
  if (enabled_) {
    dom_agent_impl_->SetDocument(document_);
    net_agent_impl_->SetDocument(document_);
  }
}

void WebDevToolsAgentImpl::DidCommitLoadForFrame(
    WebViewImpl* webview,
    WebFrame* frame,
    bool is_new_navigation) {
  if (!enabled_) {
    return;
  }
  dom_agent_impl_->DiscardBindings();
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
  if (!enabled_)
    return;
  Node* node = dom_agent_impl_->GetNodeForId(node_id);
  if (!node)
    return;
  Page* page = web_view_impl_->page();
  page->inspectorController()->highlight(node);
}

void WebDevToolsAgentImpl::HideDOMNodeHighlight() {
  Page* page = web_view_impl_->page();
  page->inspectorController()->hideHighlight();
}

void WebDevToolsAgentImpl::DispatchMessageFromClient(
    const std::string& raw_msg) {
  OwnPtr<ListValue> message(
      static_cast<ListValue*>(DevToolsRpc::ParseMessage(raw_msg)));
  if (ToolsAgentDispatch::Dispatch(this, *message.get()))
    return;

  if (!enabled_)
    return;
  if (DomAgentDispatch::Dispatch(dom_agent_impl_.get(), *message.get()))
    return;
  if (NetAgentDispatch::Dispatch(net_agent_impl_.get(), *message.get()))
    return;
}

void WebDevToolsAgentImpl::InspectElement(int x, int y) {
  Node* node = web_view_impl_->GetNodeForWindowPos(x, y);
  if (!node)
    return;

  SetEnabled(true);
  int node_id = dom_agent_impl_->PushNodePathToClient(node);
  tools_agent_delegate_stub_->UpdateFocusedNode(node_id);
}

void WebDevToolsAgentImpl::SendRpcMessage(const std::string& raw_msg) {
  delegate_->SendMessageToClient(raw_msg);
}
