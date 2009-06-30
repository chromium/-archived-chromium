// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/extensions/bindings_utils.h"

#include "base/string_util.h"
#include "chrome/renderer/render_view.h"
#include "webkit/glue/webframe.h"

namespace bindings_utils {

const char* kChromeHidden = "chromeHidden";

struct SingletonData {
  ContextList contexts;
  PendingRequestMap pending_requests;
};

// ExtensionBase

v8::Handle<v8::FunctionTemplate>
    ExtensionBase::GetNativeFunction(v8::Handle<v8::String> name) {
  if (name->Equals(v8::String::New("GetChromeHidden"))) {
    return v8::FunctionTemplate::New(GetChromeHidden);
  }

  return v8::Handle<v8::FunctionTemplate>();
}

v8::Handle<v8::Value> ExtensionBase::GetChromeHidden(
    const v8::Arguments& args) {
  v8::Local<v8::Context> context = v8::Context::GetCurrent();
  v8::Local<v8::Object> global = context->Global();
  v8::Local<v8::Value> hidden = global->GetHiddenValue(
      v8::String::New(kChromeHidden));

  if (hidden.IsEmpty() || hidden->IsUndefined()) {
    hidden = v8::Object::New();
    global->SetHiddenValue(v8::String::New(kChromeHidden), hidden);
  }

  DCHECK(hidden->IsObject());
  return hidden;
}

v8::Handle<v8::Value> ExtensionBase::StartRequest(
    const v8::Arguments& args) {
  // Get the current RenderView so that we can send a routed IPC message from
  // the correct source.
  RenderView* renderview = bindings_utils::GetRenderViewForCurrentContext();
  if (!renderview)
    return v8::Undefined();

  if (args.Length() != 3 || !args[0]->IsString() || !args[1]->IsInt32() ||
      !args[2]->IsBoolean())
    return v8::Undefined();

  std::string name = *v8::String::AsciiValue(args.Data());
  std::string json_args = *v8::String::Utf8Value(args[0]);
  int request_id = args[1]->Int32Value();
  bool has_callback = args[2]->BooleanValue();

  v8::Persistent<v8::Context> current_context =
      v8::Persistent<v8::Context>::New(v8::Context::GetCurrent());
  DCHECK(!current_context.IsEmpty());
  GetPendingRequestMap()[request_id].reset(new PendingRequest(
      current_context, *v8::String::AsciiValue(args.Data())));

  renderview->SendExtensionRequest(name, json_args, request_id, has_callback);

  return v8::Undefined();
}

ContextList& GetContexts() {
  return Singleton<SingletonData>::get()->contexts;
}

ContextList GetContextsForExtension(const std::string& extension_id) {
  ContextList& all_contexts = GetContexts();
  ContextList contexts;

  for (ContextList::iterator it = all_contexts.begin();
       it != all_contexts.end(); ++it) {
     if ((*it)->extension_id == extension_id)
       contexts.push_back(*it);
  }

  return contexts;
}

ContextList::iterator FindContext(v8::Handle<v8::Context> context) {
  ContextList& all_contexts = GetContexts();

  ContextList::iterator it = all_contexts.begin();
  for (; it != all_contexts.end(); ++it) {
     if ((*it)->context == context)
       break;
  }

  return it;
}

PendingRequestMap& GetPendingRequestMap() {
  return Singleton<SingletonData>::get()->pending_requests;
}

RenderView* GetRenderViewForCurrentContext() {
  WebFrame* webframe = WebFrame::RetrieveFrameForCurrentContext();
  DCHECK(webframe) << "RetrieveCurrentFrame called when not in a V8 context.";
  if (!webframe)
    return NULL;

  WebView* webview = webframe->GetView();
  if (!webview)
    return NULL;  // can happen during closing

  RenderView* renderview = static_cast<RenderView*>(webview->GetDelegate());
  DCHECK(renderview) << "Encountered a WebView without a WebViewDelegate";
  return renderview;
}

void CallFunctionInContext(v8::Handle<v8::Context> context,
                           const std::string& function_name, int argc,
                           v8::Handle<v8::Value>* argv) {
  v8::Context::Scope context_scope(context);

  // Look up the function name, which may be a sub-property like
  // "Port.dispatchOnMessage" in the hidden global variable.
  v8::Local<v8::Value> value =
      context->Global()->GetHiddenValue(v8::String::New(kChromeHidden));
  std::vector<std::string> components;
  SplitStringDontTrim(function_name, '.', &components);
  for (size_t i = 0; i < components.size(); ++i) {
    if (!value.IsEmpty() && value->IsObject())
      value = value->ToObject()->Get(v8::String::New(components[i].c_str()));
  }
  if (value.IsEmpty() || !value->IsFunction()) {
    NOTREACHED();
    return;
  }

  v8::Local<v8::Function> function = v8::Local<v8::Function>::Cast(value);
  if (!function.IsEmpty())
    function->Call(v8::Object::New(), argc, argv);
}

}  // namespace bindings_utils
