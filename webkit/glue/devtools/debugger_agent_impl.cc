// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include <wtf/HashSet.h>
#include <wtf/RefPtr.h>
#include <wtf/Vector.h>

#include "Document.h"
#include "Frame.h"
#include "Node.h"
#include "Page.h"
#include "PageGroup.h"
#undef LOG

#include "grit/webkit_resources.h"
#include "V8Binding.h"
#include "V8DOMWindow.h"
#include "v8_index.h"
#include "v8_proxy.h"
#include "webkit/glue/devtools/debugger_agent_impl.h"
#include "webkit/glue/devtools/debugger_agent_manager.h"
#include "webkit/glue/glue_util.h"
#include "webkit/glue/webdevtoolsagent_impl.h"
#include "webkit/glue/webkit_glue.h"
#include "webkit/glue/webview_impl.h"

using WebCore::DOMWindow;
using WebCore::Document;
using WebCore::Frame;
using WebCore::Node;
using WebCore::Page;
using WebCore::PageGroup;
using WebCore::String;
using WebCore::V8ClassIndex;
using WebCore::V8Custom;
using WebCore::V8DOMWindow;
using WebCore::V8Proxy;

DebuggerAgentImpl::DebuggerAgentImpl(
    WebViewImpl* web_view_impl,
    DebuggerAgentDelegate* delegate,
    WebDevToolsAgentImpl* webdevtools_agent)
    : web_view_impl_(web_view_impl),
      delegate_(delegate),
      webdevtools_agent_(webdevtools_agent) {
  DebuggerAgentManager::DebugAttach(this);
}

DebuggerAgentImpl::~DebuggerAgentImpl() {
  DebuggerAgentManager::DebugDetach(this);
}

void DebuggerAgentImpl::DebugBreak() {
  DebuggerAgentManager::DebugBreak(this);
}

void DebuggerAgentImpl::DebuggerOutput(const std::string& command) {
  delegate_->DebuggerOutput(command);
  webdevtools_agent_->ForceRepaint();
}

void DebuggerAgentImpl::SetDocument(Document* document) {
  v8::HandleScope scope;

  if (!document) {
    context_.Dispose();
    return;
  }

  // TODO(pfeldman): Validate against Soeren.
  // Set up the DOM window as the prototype of the new global object.
  v8::Handle<v8::Context> window_context =
      V8Proxy::GetContext(document->frame());
  v8::Handle<v8::Object> window_global = window_context->Global();
  v8::Handle<v8::Value> window_wrapper =
      V8Proxy::LookupDOMWrapper(V8ClassIndex::DOMWINDOW, window_global);

  ASSERT(V8Proxy::DOMWrapperToNative<DOMWindow>(window_wrapper) ==
      document->frame()->domWindow());

  // Create a new environment using an empty template for the shadow
  // object.  Reuse the global object if one has been created earlier.
  v8::Handle<v8::ObjectTemplate> global_template =
      V8DOMWindow::GetShadowObjectTemplate();

  // Install a security handler with V8.
  global_template->SetAccessCheckCallbacks(
      V8Custom::v8DOMWindowNamedSecurityCheck,
      V8Custom::v8DOMWindowIndexedSecurityCheck,
      v8::Integer::New(V8ClassIndex::DOMWINDOW));

  context_ = v8::Context::New(
      NULL /* no extensions */,
      global_template,
      v8::Handle<v8::Object>());
  v8::Context::Scope context_scope(context_);
  v8::Handle<v8::Object> global = context_->Global();

  v8::Handle<v8::String> implicit_proto_string = v8::String::New("__proto__");
  global->Set(implicit_proto_string, window_wrapper);

  // Give the code running in the new context a way to get access to the
  // original context.
  global->Set(v8::String::New("contentWindow"), window_global);

  // Inject javascript into the context.
  StringPiece basejs = webkit_glue::GetDataResource(IDR_DEVTOOLS_BASE_JS);
  v8::Script::Compile(v8::String::New(basejs.as_string().c_str()))->Run();
  StringPiece jsonjs = webkit_glue::GetDataResource(IDR_DEVTOOLS_JSON_JS);
  v8::Script::Compile(v8::String::New(jsonjs.as_string().c_str()))->Run();
  StringPiece injectjs = webkit_glue::GetDataResource(IDR_DEVTOOLS_INJECT_JS);
  v8::Script::Compile(v8::String::New(injectjs.as_string().c_str()))->Run();
}

String DebuggerAgentImpl::ExecuteUtilityFunction(
    const String &function_name,
    Node* node,
    const String& json_args) {
  v8::HandleScope scope;
  ASSERT(!context_.IsEmpty());
  v8::Context::Scope context_scope(context_);
  v8::Handle<v8::Function> function = v8::Local<v8::Function>::Cast(
      context_->Global()->Get(v8::String::New(function_name.utf8().data())));

  v8::Handle<v8::Value> node_wrapper =
      V8Proxy::ToV8Object(V8ClassIndex::NODE, node);
  v8::Handle<v8::String> json_args_wrapper = v8::Handle<v8::String>(
      v8::String::New(json_args.utf8().data()));
  v8::Handle<v8::Value> args[] = {
    node_wrapper,
    json_args_wrapper
  };

  v8::Handle<v8::Value> res_obj = function->Call(
      context_->Global(), 2, args);

  v8::Handle<v8::String> res_json = v8::Handle<v8::String>::Cast(res_obj);
  return WebCore::toWebCoreString(res_json);
}

void DebuggerAgentImpl::RunWithDeferredMessages(
    const HashSet<DebuggerAgentImpl*>& agents,
    WebDevToolsAgent::MessageLoopDispatchHandler handler) {

  // TODO(pfeldman): Make PageGroupLoadDeferrer visible and use it from here.
  // Code below is derived from the Chrome.cpp's PageGroupLoadDeferrer:
  // 1. Disable active objects and input events.
  Vector<RefPtr<Frame>, 16> deferred_frames;
  for (HashSet<DebuggerAgentImpl*>::const_iterator ag_it = agents.begin();
       ag_it != agents.end(); ++ag_it) {
    DebuggerAgentImpl* agent = *ag_it;
    agent->web_view()->SetIgnoreInputEvents(true);
    const HashSet<Page*>& pages = agent->GetPage()->group().pages();
    HashSet<Page*>::const_iterator end = pages.end();
    for (HashSet<Page*>::const_iterator it = pages.begin(); it != end; ++it) {
      Page* other_page = *it;
      if (!other_page->defersLoading()) {
        deferred_frames.append(other_page->mainFrame());
#if !PLATFORM(MAC)
        for (Frame* frame = other_page->mainFrame(); frame;
             frame = frame->tree()->traverseNext()) {
          frame->document()->suspendActiveDOMObjects();
        }
#endif
      }
    }
  }

  // 2. Disable loading.
  size_t count = deferred_frames.size();
  for (size_t i = 0; i < count; ++i) {
    if (Page* page = deferred_frames[i]->page()) {
       page->setDefersLoading(true);
    }
  }
  // 3. Process messages.
  handler();

  // 4. Bring things back.
  for (size_t i = 0; i < deferred_frames.size(); ++i) {
    if (Page* page = deferred_frames[i]->page()) {
      page->setDefersLoading(false);

#if !PLATFORM(MAC)
      for (Frame* frame = page->mainFrame(); frame; frame =
           frame->tree()->traverseNext()) {
        frame->document()->resumeActiveDOMObjects();
      }
#endif
    }
  }

  for (HashSet<DebuggerAgentImpl*>::const_iterator ag_it = agents.begin();
       ag_it != agents.end(); ++ag_it) {
    (*ag_it)->web_view()->SetIgnoreInputEvents(false);
  }
}

WebCore::Page* DebuggerAgentImpl::GetPage() {
  return web_view_impl_->page();
}
