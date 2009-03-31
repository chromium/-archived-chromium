// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/extensions/renderer_extension_bindings.h"

#include "chrome/common/render_messages.h"
#include "chrome/renderer/render_thread.h"
#include "third_party/WebKit/WebKit/chromium/public/WebScriptSource.h"
#include "webkit/glue/webframe.h"

using WebKit::WebScriptSource;
using WebKit::WebString;

namespace {

const char* kExtensionName = "v8/RendererExtensionBindings";

const char* kExtensionScript =
    "var chromium = chromium || {};"
    "(function () {"
    "  native function OpenChannelToExtension(id);"
    "  native function PostMessage(channel_id, msg);"
    "  chromium.Extension = function(id) {"
    "    this.channel_id_ = OpenChannelToExtension(id);"
    "    if (this.channel_id_ == -1)"
    "      throw new Error('No such extension \"' + id + '\"');"
    "    chromium.Extension.extensions_[this.channel_id_] = this;"
    "  };"
    "  chromium.Extension.extensions_ = {};"
    "  chromium.Extension.dispatchOnMessage = function(msg, channel_id) {"
    // TODO(mpcomplete): port param for onMessage
    "    var e = chromium.Extension.extensions_[channel_id];"
    "    if (e && e.onMessage) e.onMessage(msg);"
    "    if (chromium.Extension.onMessage) chromium.Extension.onMessage(msg);"
    "  };"
    "  chromium.Extension.prototype.postMessage = function(msg) {"
    "    return PostMessage(this.channel_id_, msg);"
    "  };"
    "})();";

// Message passing API example (in a content script):
// var extension =
//    new chromium.Extension('00123456789abcdef0123456789abcdef0123456');
// extension.postMessage('Can you hear me now?');
// extension.onMessage = function(msg) { alert('response=' + msg); }

class ExtensionImpl : public v8::Extension {
 public:
  ExtensionImpl() : v8::Extension(kExtensionName, kExtensionScript) {}
  ~ExtensionImpl() {}

  virtual v8::Handle<v8::FunctionTemplate> GetNativeFunction(
      v8::Handle<v8::String> name) {
    if (name->Equals(v8::String::New("OpenChannelToExtension"))) {
      return v8::FunctionTemplate::New(OpenChannelToExtension);
    } else if (name->Equals(v8::String::New("PostMessage"))) {
      return v8::FunctionTemplate::New(PostMessage);
    }
    return v8::Handle<v8::FunctionTemplate>();
  }
  static v8::Handle<v8::Value> OpenChannelToExtension(
      const v8::Arguments& args) {
    if (args.Length() >= 1 && args[0]->IsString()) {
      std::string id = *v8::String::Utf8Value(args[0]->ToString());
      int channel_id;
      RenderThread::current()->Send(
          new ViewHostMsg_OpenChannelToExtension(id, &channel_id));
      return v8::Integer::New(channel_id);
      // TODO(mpcomplete): should we associate channel_id with the frame it
      // came from, so we can run the onmessage handler in that context for
      // responses?
    }
    return v8::Undefined();
  }
  static v8::Handle<v8::Value> PostMessage(const v8::Arguments& args) {
    if (args.Length() >= 2 && args[0]->IsInt32() && args[1]->IsString()) {
      int channel_id = args[1]->Int32Value();
      std::string message = *v8::String::Utf8Value(args[1]->ToString());
      RenderThread::current()->Send(
          new ViewHostMsg_ExtensionPostMessage(channel_id, message));
    }
    return v8::Undefined();
  }
};

}  // namespace

namespace extensions_v8 {

v8::Extension* RendererExtensionBindings::Get() {
  return new ExtensionImpl();
}

void RendererExtensionBindings::HandleExtensionMessage(
    WebFrame* webframe, const std::string& message, int channel_id) {
  // TODO(mpcomplete): escape message
  std::string script = StringPrintf(
      "void(chromium.Extension.dispatchOnMessage(\"%s\", %d))",
      message.c_str(), channel_id);
  webframe->ExecuteScript(WebScriptSource(WebString::fromUTF8(script)));
}

}  // namespace extensions_v8
