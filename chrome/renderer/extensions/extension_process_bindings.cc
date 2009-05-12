// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/extensions/extension_process_bindings.h"

#include "base/singleton.h"
#include "chrome/common/render_messages.h"
#include "chrome/renderer/extensions/bindings_utils.h"
#include "chrome/renderer/extensions/event_bindings.h"
#include "chrome/renderer/extensions/renderer_extension_bindings.h"
#include "chrome/renderer/js_only_v8_extensions.h"
#include "chrome/renderer/render_view.h"
#include "grit/renderer_resources.h"
#include "webkit/api/public/WebScriptSource.h"
#include "webkit/glue/webframe.h"

using WebKit::WebScriptSource;
using WebKit::WebString;

namespace {

const char kExtensionName[] = "chrome/ExtensionProcessBindings";
const char* kExtensionDeps[] = {
  BaseJsV8Extension::kName,
  EventBindings::kName,
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
    std::set<std::string>* name_set = GetFunctionNameSet();
    for (size_t i = 0; i < names.size(); ++i) {
      name_set->insert(names[i]);
    }
  }

  virtual v8::Handle<v8::FunctionTemplate> GetNativeFunction(
      v8::Handle<v8::String> name) {
    std::set<std::string>* names = GetFunctionNameSet();

    if (name->Equals(v8::String::New("GetNextRequestId")))
      return v8::FunctionTemplate::New(GetNextRequestId);
    else if (names->find(*v8::String::AsciiValue(name)) != names->end())
      return v8::FunctionTemplate::New(StartRequest, name);

    return v8::Handle<v8::FunctionTemplate>();
  }

 private:
  struct SingletonData {
    std::set<std::string> function_names_;
  };

  static std::set<std::string>* GetFunctionNameSet() {
    return &Singleton<SingletonData>()->function_names_;
  }

  static v8::Handle<v8::Value> GetNextRequestId(const v8::Arguments& args) {
    static int next_request_id = 0;
    return v8::Integer::New(next_request_id++);
  }

  static v8::Handle<v8::Value> StartRequest(const v8::Arguments& args) {
    // Get the current RenderView so that we can send a routed IPC message from
    // the correct source.
    WebFrame* webframe = WebFrame::RetrieveFrameForCurrentContext();
    RenderView* renderview = GetRenderViewForCurrentContext();
    if (!webframe || !renderview)
      return v8::Undefined();

    if (args.Length() != 3 || !args[0]->IsString() || !args[1]->IsInt32() ||
        !args[2]->IsBoolean())
      return v8::Undefined();

    int request_id = args[1]->Int32Value();
    bool has_callback = args[2]->BooleanValue();

    renderview->SendExtensionRequest(
        std::string(*v8::String::AsciiValue(args.Data())),
        std::string(*v8::String::Utf8Value(args[0])),
        request_id, has_callback, webframe);

    return v8::Undefined();
  }
};

}  // namespace

v8::Extension* ExtensionProcessBindings::Get() {
  return new ExtensionImpl();
}

void ExtensionProcessBindings::SetFunctionNames(
    const std::vector<std::string>& names) {
  ExtensionImpl::SetFunctionNames(names);
}

void ExtensionProcessBindings::ExecuteResponseInFrame(
    CallContext *call, int request_id, bool success,
    const std::string& response,
    const std::string& error) {
  std::string code = "chrome.handleResponse_(";
  code += IntToString(request_id);

  code += ", '" + call->name_;

  if (success)
    code += "', true";
  else
    code += "', false";

  code += ", '";
  size_t offset = code.length();
  code += response;
  ReplaceSubstringsAfterOffset(&code, offset, "\\", "\\\\");
  ReplaceSubstringsAfterOffset(&code, offset, "'", "\\'");
  code += "', '";
  offset = code.length();
  code += error;
  ReplaceSubstringsAfterOffset(&code, offset, "\\", "\\\\");
  ReplaceSubstringsAfterOffset(&code, offset, "'", "\\'");
  code += "')";

  call->frame_->ExecuteScript(WebScriptSource(WebString::fromUTF8(code)));
}
