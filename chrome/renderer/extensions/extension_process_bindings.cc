// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/extensions/extension_process_bindings.h"

#include "chrome/common/render_messages.h"
#include "chrome/renderer/extensions/bindings_utils.h"
#include "chrome/renderer/extensions/renderer_extension_bindings.h"
#include "chrome/renderer/js_only_v8_extensions.h"
#include "chrome/renderer/render_view.h"
#include "grit/renderer_resources.h"
#include "third_party/WebKit/WebKit/chromium/public/WebScriptSource.h"
#include "webkit/glue/webframe.h"

using WebKit::WebScriptSource;
using WebKit::WebString;

namespace {

const char kExtensionName[] = "chrome/ExtensionProcessBindings";
const char* kExtensionDeps[] = {
  BaseJsV8Extension::kName,
  JsonJsV8Extension::kName,
  JsonSchemaJsV8Extension::kName,
  RendererExtensionBindings::kName,
};

class ExtensionImpl : public v8::Extension {
 public:
  ExtensionImpl() : v8::Extension(
      kExtensionName, GetStringResource<IDR_EXTENSION_PROCESS_BINDINGS_JS>(),
      arraysize(kExtensionDeps), kExtensionDeps) {}

  static void SetFunctionNames(const std::vector<std::string>& names) {
    function_names_ = new std::set<std::string>();
    for (size_t i = 0; i < names.size(); ++i) {
      function_names_->insert(names[i]);
    }
  }

  virtual v8::Handle<v8::FunctionTemplate> GetNativeFunction(
      v8::Handle<v8::String> name) {
    if (name->Equals(v8::String::New("GetNextCallbackId")))
      return v8::FunctionTemplate::New(GetNextCallbackId);
    else if (function_names_->find(*v8::String::AsciiValue(name)) !=
             function_names_->end())
      return v8::FunctionTemplate::New(StartRequest, name);

    return v8::Handle<v8::FunctionTemplate>();
  }

 private:
  static v8::Handle<v8::Value> GetNextCallbackId(const v8::Arguments& args) {
    static int next_callback_id = 0;
    return v8::Integer::New(next_callback_id++);
  }

  static v8::Handle<v8::Value> StartRequest(const v8::Arguments& args) {
    WebFrame* webframe = WebFrame::RetrieveActiveFrame();
    DCHECK(webframe) << "There should be an active frame since we just got "
                        "a native function called.";
    if (!webframe) return v8::Undefined();

    WebView* webview = webframe->GetView();
    if (!webview) return v8::Undefined();  // can happen during closing

    RenderView* renderview = static_cast<RenderView*>(webview->GetDelegate());
    DCHECK(renderview) << "Encountered a WebView without a WebViewDelegate";
    if (!renderview) return v8::Undefined();

    if (args.Length() != 2 || !args[0]->IsString() || !args[1]->IsInt32())
      return v8::Undefined();

    int callback_id = args[1]->Int32Value();
    renderview->SendExtensionRequest(
        std::string(*v8::String::AsciiValue(args.Data())),
        std::string(*v8::String::Utf8Value(args[0])),
        callback_id, webframe);

    return v8::Undefined();
  }

  static std::set<std::string>* function_names_;
};

std::set<std::string>* ExtensionImpl::function_names_;

}  // namespace

v8::Extension* ExtensionProcessBindings::Get() {
  return new ExtensionImpl();
}

void ExtensionProcessBindings::SetFunctionNames(
    const std::vector<std::string>& names) {
  ExtensionImpl::SetFunctionNames(names);
}

void ExtensionProcessBindings::ExecuteCallbackInFrame(
    WebFrame* frame, int callback_id, const std::string& response) {
  std::string code = "chromium._dispatchCallback(";
  code += IntToString(callback_id);
  code += ", '";

  size_t offset = code.length();
  code += response;
  ReplaceSubstringsAfterOffset(&code, offset, "\\", "\\\\");
  ReplaceSubstringsAfterOffset(&code, offset, "'", "\\'");
  code += "')";

  frame->ExecuteScript(WebScriptSource(WebString::fromUTF8(code)));
}
