// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/extensions/renderer_extension_bindings.h"

#include "app/resource_bundle.h"
#include "base/basictypes.h"
#include "chrome/common/render_messages.h"
#include "chrome/renderer/extensions/bindings_utils.h"
#include "chrome/renderer/extensions/event_bindings.h"
#include "chrome/renderer/render_thread.h"
#include "chrome/renderer/render_view.h"
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

const char* kExtensionDeps[] = { EventBindings::kName };

class ExtensionImpl : public v8::Extension {
 public:
  ExtensionImpl()
      : v8::Extension(RendererExtensionBindings::kName,
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
    RenderView* renderview = GetActiveRenderView();
    if (!renderview)
      return v8::Undefined();

    if (args.Length() >= 1 && args[0]->IsString()) {
      std::string id = *v8::String::Utf8Value(args[0]->ToString());
      int port_id = -1;
      renderview->Send(new ViewHostMsg_OpenChannelToExtension(
          renderview->routing_id(), id, &port_id));
      return v8::Integer::New(port_id);
    }
    return v8::Undefined();
  }

  // Sends a message along the given channel.
  static v8::Handle<v8::Value> PostMessage(const v8::Arguments& args) {
    RenderView* renderview = GetActiveRenderView();
    if (!renderview)
      return v8::Undefined();

    if (args.Length() >= 2 && args[0]->IsInt32() && args[1]->IsString()) {
      int port_id = args[0]->Int32Value();
      std::string message = *v8::String::Utf8Value(args[1]->ToString());
      renderview->Send(new ViewHostMsg_ExtensionPostMessage(
          renderview->routing_id(), port_id, message));
    }
    return v8::Undefined();
  }
};

}  // namespace

const char* RendererExtensionBindings::kName =
    "chrome/RendererExtensionBindings";

v8::Extension* RendererExtensionBindings::Get() {
  return new ExtensionImpl();
}

void RendererExtensionBindings::HandleConnect(int port_id,
                                              const std::string& tab_json) {
  v8::HandleScope handle_scope;
  v8::Handle<v8::Value> argv[2];
  argv[0] = v8::Integer::New(port_id);
  argv[1] = v8::String::New(tab_json.c_str());
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

void RendererExtensionBindings::HandleEvent(const std::string& event_name,
                                            const std::string& args) {
  v8::HandleScope handle_scope;
  v8::Handle<v8::Value> argv[2];
  argv[0] = v8::String::New(event_name.c_str());
  argv[1] = v8::String::New(args.c_str());

  EventBindings::CallFunction("chromium.Event.dispatchJSON_",
                              arraysize(argv), argv);
}
