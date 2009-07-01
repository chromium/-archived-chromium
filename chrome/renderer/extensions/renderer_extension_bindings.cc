// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/extensions/renderer_extension_bindings.h"

#include "app/resource_bundle.h"
#include "base/basictypes.h"
#include "base/values.h"
#include "chrome/common/render_messages.h"
#include "chrome/renderer/extensions/bindings_utils.h"
#include "chrome/renderer/extensions/event_bindings.h"
#include "chrome/renderer/render_thread.h"
#include "chrome/renderer/render_view.h"
#include "grit/renderer_resources.h"

using bindings_utils::GetStringResource;
using bindings_utils::ExtensionBase;

// Message passing API example (in a content script):
// var extension =
//    new chrome.Extension('00123456789abcdef0123456789abcdef0123456');
// var port = extension.connect();
// port.postMessage('Can you hear me now?');
// port.onmessage.addListener(function(msg, port) {
//   alert('response=' + msg);
//   port.postMessage('I got your reponse');
// });

namespace {

const char* kExtensionDeps[] = { EventBindings::kName };

class ExtensionImpl : public ExtensionBase {
 public:
  ExtensionImpl()
      : ExtensionBase(RendererExtensionBindings::kName,
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
    } else if (name->Equals(v8::String::New("CloseChannel"))) {
      return v8::FunctionTemplate::New(CloseChannel);
    }
    return ExtensionBase::GetNativeFunction(name);
  }

  // Creates a new messaging channel to the given extension.
  static v8::Handle<v8::Value> OpenChannelToExtension(
      const v8::Arguments& args) {
    // Get the current RenderView so that we can send a routed IPC message from
    // the correct source.
    RenderView* renderview = bindings_utils::GetRenderViewForCurrentContext();
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
    RenderView* renderview = bindings_utils::GetRenderViewForCurrentContext();
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

  // Sends a message along the given channel.
  static v8::Handle<v8::Value> CloseChannel(const v8::Arguments& args) {
    if (args.Length() >= 1 && args[0]->IsInt32()) {
      int port_id = args[0]->Int32Value();
      // Send via the RenderThread because the RenderView might be closing.
      EventBindings::GetRenderThread()->Send(
          new ViewHostMsg_ExtensionCloseChannel(port_id));
    }
    return v8::Undefined();
  }
};

// Convert a ListValue to a vector of V8 values.
static std::vector< v8::Handle<v8::Value> > ListValueToV8(
    const ListValue& value) {
  std::vector< v8::Handle<v8::Value> > v8_values;

  for (size_t i = 0; i < value.GetSize(); ++i) {
    Value* elem = NULL;
    value.Get(i, &elem);
    switch (elem->GetType()) {
      case Value::TYPE_NULL:
        v8_values.push_back(v8::Null());
        break;
      case Value::TYPE_BOOLEAN: {
        bool val;
        elem->GetAsBoolean(&val);
        v8_values.push_back(v8::Boolean::New(val));
        break;
      }
      case Value::TYPE_INTEGER: {
        int val;
        elem->GetAsInteger(&val);
        v8_values.push_back(v8::Integer::New(val));
        break;
      }
      case Value::TYPE_REAL: {
        double val;
        elem->GetAsReal(&val);
        v8_values.push_back(v8::Number::New(val));
        break;
      }
      case Value::TYPE_STRING: {
        std::string val;
        elem->GetAsString(&val);
        v8_values.push_back(v8::String::New(val.c_str()));
        break;
      }
      default:
        NOTREACHED() << "Unsupported Value type.";
        break;
    }
  }

  return v8_values;
}

}  // namespace

const char* RendererExtensionBindings::kName =
    "chrome/RendererExtensionBindings";

v8::Extension* RendererExtensionBindings::Get() {
  return new ExtensionImpl();
}

void RendererExtensionBindings::Invoke(const std::string& function_name,
                                       const ListValue& args) {
  v8::HandleScope handle_scope;
  std::vector< v8::Handle<v8::Value> > argv = ListValueToV8(args);
  EventBindings::CallFunction(function_name, argv.size(), &argv[0]);
}
