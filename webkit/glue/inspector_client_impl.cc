// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "base/compiler_specific.h"

MSVC_PUSH_WARNING_LEVEL(0);
#include "DOMWindow.h"
#include "FloatRect.h"
#include "InspectorController.h"
#include "Page.h"
#include "Settings.h"
MSVC_POP_WARNING();

#undef LOG
#include "webkit/glue/inspector_client_impl.h"
#include "webkit/glue/webkit_glue.h"
#include "webkit/glue/webview_impl.h"
#include "googleurl/src/gurl.h"
#include "net/base/net_util.h"

using namespace WebCore;

static const float kDefaultInspectorXPos = 10;
static const float kDefaultInspectorYPos = 50;
static const float kDefaultInspectorHeight = 640;
static const float kDefaultInspectorWidth = 480;

WebInspectorClient::WebInspectorClient(WebView* webView)
  : inspected_web_view_(webView)
  , inspector_web_view_(0) {
  ASSERT(inspected_web_view_);
}

WebInspectorClient::~WebInspectorClient() {
}

void WebInspectorClient::inspectorDestroyed() {
  delete this;
}

Page* WebInspectorClient::createPage() {
  WebCore::Page* page;

  if (inspector_web_view_ != NULL) {
    page = static_cast<WebViewImpl*>(inspector_web_view_)->page();
    ASSERT(page != NULL);
    if (page != NULL)
      return page;
  }

  WebViewDelegate* delegate = inspected_web_view_->GetDelegate();
  if (!delegate)
    return NULL;
  inspector_web_view_ = delegate->CreateWebView(inspected_web_view_, true);
  if (!inspector_web_view_)
    return NULL;

  GURL inspector_url(webkit_glue::GetInspectorURL());
  scoped_ptr<WebRequest> request(WebRequest::Create(inspector_url));
  WebViewImpl* inspector_web_view_impl =
    static_cast<WebViewImpl*>(inspector_web_view_);
  inspector_web_view_impl->main_frame()->LoadRequest(request.get());

  page = inspector_web_view_impl->page();

  page->chrome()->setToolbarsVisible(false);
  page->chrome()->setStatusbarVisible(false);
  page->chrome()->setScrollbarsVisible(false);
  page->chrome()->setMenubarVisible(false);
  page->chrome()->setResizable(true);

  // Don't allow inspection of inspector.
  page->settings()->setDeveloperExtrasEnabled(false);
  page->settings()->setPrivateBrowsingEnabled(true);
  page->settings()->setPluginsEnabled(false);
  page->settings()->setJavaEnabled(false);

  FloatRect windowRect = page->chrome()->windowRect();
  FloatSize pageSize = page->chrome()->pageRect().size();
  windowRect.setX(kDefaultInspectorXPos);
  windowRect.setY(kDefaultInspectorYPos);
  windowRect.setWidth(kDefaultInspectorHeight);
  windowRect.setHeight(kDefaultInspectorWidth);
  page->chrome()->setWindowRect(windowRect);

  page->chrome()->show();

  return page;
}

void WebInspectorClient::showWindow() {
  WebViewImpl* impl = static_cast<WebViewImpl*>(inspected_web_view_.get());
  InspectorController* inspector = impl->page()->inspectorController();
  inspector->setWindowVisible(true);

  // Notify the webview delegate of how many resources we're inspecting.
  WebViewDelegate* d = impl->delegate();
  DCHECK(d);
  d->WebInspectorOpened(inspector->resources().size());
}

void WebInspectorClient::closeWindow() {
  inspector_web_view_ = NULL;
  WebViewImpl* impl = static_cast<WebViewImpl*>(inspected_web_view_.get());
  WebFrameImpl* frame = static_cast<WebFrameImpl*>(impl->GetMainFrame());

  if (frame && frame->inspected_node())
    hideHighlight();

  if (impl->page())
    impl->page()->inspectorController()->setWindowVisible(false);
}

bool WebInspectorClient::windowVisible() {
  if (inspector_web_view_ != NULL) {
    Page* page = static_cast<WebViewImpl*>(inspector_web_view_)->page();
    ASSERT(page != NULL);
    if (page != NULL)
      return true;
  }
  return false;
}

void WebInspectorClient::attachWindow() {
  // TODO(jackson): Implement this
}

void WebInspectorClient::detachWindow() {
  // TODO(jackson): Implement this
}

void WebInspectorClient::setAttachedWindowHeight(unsigned int height) {
  // TODO(dglazkov): Implement this
  NOTIMPLEMENTED();
}

static void invalidateNodeBoundingRect(WebViewImpl* web_view) {
  // TODO(ojan): http://b/1143996 Is it important to just invalidate the rect
  // of the node region given that this is not on a critical codepath?
  // In order to do so, we'd have to take scrolling into account.
  gfx::Size size = web_view->size();
  gfx::Rect damaged_rect(0, 0, size.width(), size.height());
  web_view->GetDelegate()->DidInvalidateRect(web_view, damaged_rect);
}

void WebInspectorClient::highlight(Node* node) {
  WebViewImpl* web_view = static_cast<WebViewImpl*>(inspected_web_view_.get());
  WebFrameImpl* frame = static_cast<WebFrameImpl*>(web_view->GetMainFrame());

  if (frame->inspected_node())
    hideHighlight();

  invalidateNodeBoundingRect(web_view);
  frame->selectNodeFromInspector(node);
}

void WebInspectorClient::hideHighlight() {
  WebViewImpl* web_view = static_cast<WebViewImpl*>(inspected_web_view_.get());
  WebFrameImpl* frame = static_cast<WebFrameImpl*>(web_view->GetMainFrame());

  invalidateNodeBoundingRect(web_view);
  frame->selectNodeFromInspector(NULL);
}

void WebInspectorClient::inspectedURLChanged(const String& newURL) {
  // TODO(jackson): Implement this
}

String WebInspectorClient::localizedStringsURL() {
  NOTIMPLEMENTED();
  return String();
}

void WebInspectorClient::populateSetting(
    const String& key, InspectorController::Setting&) {
  NOTIMPLEMENTED();
}

void WebInspectorClient::storeSetting(
    const String& key, const InspectorController::Setting&) {
  NOTIMPLEMENTED();
}

void WebInspectorClient::removeSetting(const String& key) {
  NOTIMPLEMENTED();
}
