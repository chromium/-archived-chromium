// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/external_extension.h"
#include "chrome/renderer/render_view.h"
#include "webkit/glue/webframe.h"

namespace extensions_v8 {

const char* kExternalExtensionName = "v8/External";

class ExternalExtensionWrapper : public v8::Extension {
 public:
  ExternalExtensionWrapper()
      : v8::Extension(
            kExternalExtensionName,
            "var external;"
            "if (!external)"
            "  external = {};"
            "external.AddSearchProvider = function(name) {"
            "  native function NativeAddSearchProvider();"
            "  NativeAddSearchProvider(name);"
            "}") {}

    virtual v8::Handle<v8::FunctionTemplate> GetNativeFunction(
        v8::Handle<v8::String> name) {
      if (name->Equals(v8::String::New("NativeAddSearchProvider"))) {
        return v8::FunctionTemplate::New(AddSearchProvider);
      }
      return v8::Handle<v8::FunctionTemplate>();
    }

    static v8::Handle<v8::Value> AddSearchProvider(const v8::Arguments& args) {
      if (!args.Length())
        return v8::Undefined();

      WebFrame* webframe = WebFrame::RetrieveFrameForEnteredContext();
      DCHECK(webframe) << "There should be an active frame since we just got "
                          "a native function called.";
      if (!webframe) return v8::Undefined();

      WebView* webview = webframe->GetView();
      if (!webview) return v8::Undefined();  // can happen during closing

      RenderView* renderview = static_cast<RenderView*>(webview->GetDelegate());
      if (!renderview) return v8::Undefined();

      std::string name = std::string(*v8::String::Utf8Value(args[0]));
      if (!name.length()) return v8::Undefined();;

      renderview->AddSearchProvider(name);
      return v8::Undefined();
    }
};

v8::Extension*  ExternalExtension::Get() {
  return new ExternalExtensionWrapper();
}

}  // namespace extensions_v8
