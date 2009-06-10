// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/extensions/bindings_utils.h"

#include "base/string_util.h"
#include "chrome/renderer/render_view.h"
#include "webkit/glue/webframe.h"

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
  // "chrome.handleResponse_" in the global variable.
  v8::Local<v8::Value> value = context->Global();
  std::vector<std::string> components;
  SplitStringDontTrim(function_name, '.', &components);
  for (size_t i = 0; i < components.size(); ++i) {
    if (value->IsObject())
      value = value->ToObject()->Get(v8::String::New(components[i].c_str()));
  }
  if (!value->IsFunction())
    return;

  v8::Local<v8::Function> function = v8::Local<v8::Function>::Cast(value);
  if (!function.IsEmpty())
    function->Call(v8::Object::New(), argc, argv);
}
