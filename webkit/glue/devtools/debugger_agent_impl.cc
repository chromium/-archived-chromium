// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "Document.h"
#include "Node.h"
#undef LOG

#include "grit/webkit_resources.h"
#include "V8Binding.h"
#include "v8_index.h"
#include "v8_proxy.h"
#include "webkit/glue/devtools/debugger_agent_impl.h"
#include "webkit/glue/devtools/debugger_agent_manager.h"
#include "webkit/glue/glue_util.h"
#include "webkit/glue/webkit_glue.h"

using WebCore::Document;
using WebCore::Node;
using WebCore::String;
using WebCore::V8ClassIndex;
using WebCore::V8Proxy;

DebuggerAgentImpl::DebuggerAgentImpl(DebuggerAgentDelegate* delegate)
    : delegate_(delegate) {
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
}

void DebuggerAgentImpl::SetDocument(Document* document) {
  v8::HandleScope scope;
  v8::Handle<v8::ObjectTemplate> global_template = v8::ObjectTemplate::New();
  if (!document) {
    context_ = v8::Context::New(NULL /* no extensions */, global_template);
    return;
  }

  // TODO(pfeldman): Do not modify existing context - introduce utility one
  // instead.
  context_ = v8::Persistent<v8::Context>::New(
      V8Proxy::GetContext(document->frame()));
  v8::Context::Scope context_scope(context_);

  std::string basejs = webkit_glue::GetDataResource(IDR_DEVTOOLS_BASE_JS);
  v8::Script::Compile(v8::String::New(basejs.c_str()))->Run();
  std::string jsonjs = webkit_glue::GetDataResource(IDR_DEVTOOLS_JSON_JS);
  v8::Script::Compile(v8::String::New(jsonjs.c_str()))->Run();
  std::string injectjs = webkit_glue::GetDataResource(IDR_DEVTOOLS_INJECT_JS);
  v8::Script::Compile(v8::String::New(injectjs.c_str()))->Run();
}

String DebuggerAgentImpl::ExecuteUtilityFunction(
    const String &function_name,
    Node* node,
    const String& json_args) {
  v8::HandleScope scope;
  v8::Context::Scope context_scope(v8::Local<v8::Context>::New(context_));
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
