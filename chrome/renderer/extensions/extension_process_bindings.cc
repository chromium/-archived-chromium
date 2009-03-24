// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/extensions/extension_process_bindings.h"

#include "chrome/common/render_messages.h"
#include "chrome/common/resource_bundle.h"
#include "chrome/renderer/render_view.h"
#include "grit/renderer_resources.h"
#include "webkit/glue/webframe.h"

namespace extensions_v8 {

const char kExtensionProcessExtensionName[] = "v8/ExtensionProcess";

class ExtensionProcessBindingsWrapper : public v8::Extension {
 public:
  ExtensionProcessBindingsWrapper()
      : v8::Extension(kExtensionProcessExtensionName, GetSource()) {}

  virtual v8::Handle<v8::FunctionTemplate> GetNativeFunction(
      v8::Handle<v8::String> name) {
    if (name->Equals(v8::String::New("GetNextCallbackId")))
      return v8::FunctionTemplate::New(GetNextCallbackId);
    else if (name->Equals(v8::String::New("CreateTab")))
      return v8::FunctionTemplate::New(StartRequest, name);

    return v8::Handle<v8::FunctionTemplate>();
  }

 private:
   static const char* GetSource() {
    // This is weird. The v8::Extension constructor expects a null-terminated
    // string which it doesn't own (all current uses are constant). The value
    // returned by GetDataResource is *not* null-terminated, and simply
    // converting it to a string, then using that string's c_str() in this
    // class's constructor doesn't work because the resulting string is stack-
    // allocated.
     if (!source_)
      source_ = new std::string(
          ResourceBundle::GetSharedInstance().GetRawDataResource(
              IDR_EXTENSION_PROCESS_BINDINGS_JS).as_string());

    return source_->c_str();
  }

  static v8::Handle<v8::Value> GetNextCallbackId(const v8::Arguments& args) {
    static int next_callback_id = 0;
    return v8::Integer::New(next_callback_id++);
  }

  static v8::Handle<v8::Value> StartRequest(const v8::Arguments& args) {
    WebFrame* webframe = WebFrame::RetrieveActiveFrame();
    DCHECK(webframe) << "There should be an active frame since we just got "
                        "an API called.";
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

  static std::string* source_;
};

std::string* ExtensionProcessBindingsWrapper::source_;


// static
v8::Extension* ExtensionProcessBindings::Get() {
  return new ExtensionProcessBindingsWrapper();
}

// static
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

  frame->ExecuteScript(webkit_glue::WebScriptSource(code));
}

}  // namespace extensions_v8
