// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/extensions/renderer_extension_bindings.h"

#include "base/basictypes.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/resource_bundle.h"
#include "chrome/renderer/extensions/bindings_utils.h"
#include "chrome/renderer/extensions/event_bindings.h"
#include "chrome/renderer/render_thread.h"
#include "grit/renderer_resources.h"

// Message passing API example (in a content script):
// var extension =
//    new chromium.Extension('00123456789abcdef0123456789abcdef0123456');
// var port = extension.connect();
// port.postMessage('Can you hear me now?');
// port.onmessage.addListener(function(msg, port) {
//   alert('response=' + msg);
//   port.postMessage('I got your reponse');
// });

namespace {

// We use the generic interface so that unit tests can inject a mock.
RenderThreadBase* render_thread_ = NULL;

const char* kExtensionName = "v8/RendererExtensionBindings";
const char* kExtensionDeps[] = { EventBindings::kName };

class ExtensionImpl : public v8::Extension {
 public:
  ExtensionImpl()
      : v8::Extension(kExtensionName,
                      GetStringResource<IDR_RENDERER_EXTENSION_BINDINGS_JS>(),
                      arraysize(kExtensionDeps), kExtensionDeps) {
  }
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

  // Creates a new messaging channel to the given extension.
  static v8::Handle<v8::Value> OpenChannelToExtension(
      const v8::Arguments& args) {
    if (args.Length() >= 1 && args[0]->IsString()) {
      std::string id = *v8::String::Utf8Value(args[0]->ToString());
      int port_id = -1;
      render_thread_->Send(
          new ViewHostMsg_OpenChannelToExtension(id, &port_id));
      return v8::Integer::New(port_id);
    }
    return v8::Undefined();
  }

  // Sends a message along the given channel.
  static v8::Handle<v8::Value> PostMessage(const v8::Arguments& args) {
    if (args.Length() >= 2 && args[0]->IsInt32() && args[1]->IsString()) {
      int port_id = args[0]->Int32Value();
      std::string message = *v8::String::Utf8Value(args[1]->ToString());
      render_thread_->Send(
          new ViewHostMsg_ExtensionPostMessage(port_id, message));
    }
    return v8::Undefined();
  }
};

}  // namespace

namespace extensions_v8 {

v8::Extension* RendererExtensionBindings::Get(RenderThreadBase* render_thread) {
  render_thread_ = render_thread;
  return new ExtensionImpl();
}

void RendererExtensionBindings::HandleConnect(int port_id) {
  v8::HandleScope handle_scope;
  v8::Handle<v8::Value> argv[1];
  argv[0] = v8::Integer::New(port_id);
  EventBindings::CallFunction("chromium.Port.dispatchOnConnect_",
                              arraysize(argv), argv);
}

void RendererExtensionBindings::HandleMessage(const std::string& message,
                                              int port_id) {
  v8::HandleScope handle_scope;
  v8::Handle<v8::Value> argv[2];
  argv[0] = v8::String::New(message.c_str());
  argv[1] = v8::Integer::New(port_id);
  EventBindings::CallFunction("chromium.Port.dispatchOnMessage_",
                              arraysize(argv), argv);
}

}  // namespace extensions_v8
