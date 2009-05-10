// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/render_view_test.h"

#include "chrome/common/render_messages.h"
#include "chrome/browser/extensions/extension_function_dispatcher.h"
#include "chrome/renderer/extensions/event_bindings.h"
#include "chrome/renderer/extensions/extension_process_bindings.h"
#include "chrome/renderer/extensions/renderer_extension_bindings.h"
#include "chrome/renderer/js_only_v8_extensions.h"
#include "webkit/api/public/WebKit.h"
#include "webkit/api/public/WebScriptSource.h"
#include "webkit/glue/weburlrequest.h"
#include "webkit/glue/webview.h"

using WebKit::WebScriptSource;
using WebKit::WebString;

namespace {

const int32 kRouteId = 5;
const int32 kOpenerId = 7;

};

void RenderViewTest::ProcessPendingMessages() {
  msg_loop_.PostTask(FROM_HERE, new MessageLoop::QuitTask());
  msg_loop_.Run();
}

WebFrame* RenderViewTest::GetMainFrame() {
  return view_->webview()->GetMainFrame();
}

void RenderViewTest::ExecuteJavaScript(const char* js) {
  GetMainFrame()->ExecuteScript(WebScriptSource(WebString::fromUTF8(js)));
}

void RenderViewTest::LoadHTML(const char* html) {
  std::string url_str = "data:text/html;charset=utf-8,";
  url_str.append(html);
  GURL url(url_str);

  scoped_ptr<WebRequest> request(WebRequest::Create(url));
  GetMainFrame()->LoadRequest(request.get());

  // The load actually happens asynchronously, so we pump messages to process
  // the pending continuation.
  ProcessPendingMessages();
}

void RenderViewTest::SetUp() {
  WebKit::initialize(&webkitclient_);
  WebKit::registerExtension(BaseJsV8Extension::Get());
  WebKit::registerExtension(JsonJsV8Extension::Get());
  WebKit::registerExtension(JsonSchemaJsV8Extension::Get());
  WebKit::registerExtension(EventBindings::Get());
  WebKit::registerExtension(ExtensionProcessBindings::Get());
  WebKit::registerExtension(RendererExtensionBindings::Get());
  EventBindings::SetRenderThread(&render_thread_);

  // TODO(aa): Should some of this go to some other inheriting class?
  std::vector<std::string> names;
  ExtensionFunctionDispatcher::GetAllFunctionNames(&names);
  ExtensionProcessBindings::SetFunctionNames(names);

  mock_process_.reset(new MockProcess());

  render_thread_.set_routing_id(kRouteId);

  // This needs to pass the mock render thread to the view.
  view_ = RenderView::Create(&render_thread_, NULL, NULL, kOpenerId,
                             WebPreferences(),
                             new SharedRenderViewCounter(0), kRouteId);
}
void RenderViewTest::TearDown() {
  render_thread_.SendCloseMessage();

  // Run the loop so the release task from the renderwidget executes.
  ProcessPendingMessages();

  EventBindings::SetRenderThread(NULL);

  view_ = NULL;

  mock_process_.reset();
  WebKit::shutdown();

  msg_loop_.RunAllPending();
}
