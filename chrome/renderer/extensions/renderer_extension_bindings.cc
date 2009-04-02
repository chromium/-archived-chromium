// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/extensions/renderer_extension_bindings.h"

#include "base/basictypes.h"
#include "base/singleton.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/resource_bundle.h"
#include "chrome/renderer/render_thread.h"
#include "grit/renderer_resources.h"
#include "webkit/glue/webframe.h"

// Message passing API example (in a content script):
// var extension =
//    new chromium.Extension('00123456789abcdef0123456789abcdef0123456');
// var channel = extension.openChannel();
// channel.postMessage('Can you hear me now?');
// channel.onMessage = function(msg, port) {
//   alert('response=' + msg);
//   port.postMessage('I got your reponse');
// }

namespace {

// Keep a list of contexts that have registered themselves with us.  This lets
// us know where to dispatch events when we receive them.
typedef std::list< v8::Persistent<v8::Context> > ContextList;
struct SingletonData {
  ContextList contexts;
  std::string js_source;

  SingletonData() :
    js_source(ResourceBundle::GetSharedInstance().GetRawDataResource(
        IDR_RENDERER_EXTENSION_BINDINGS_JS).as_string()) {
  }
};
ContextList& GetRegisteredContexts() {
  return Singleton<SingletonData>::get()->contexts;
}
const char* GetSource() {
  return Singleton<SingletonData>::get()->js_source.c_str();
}

// We use the generic interface so that unit tests can inject a mock.
RenderThreadBase* render_thread_ = NULL;

const char* kExtensionName = "v8/RendererExtensionBindings";
const char* kScriptAPI = "chromium.extensions.scriptAPI";

class ExtensionImpl : public v8::Extension {
 public:
  ExtensionImpl() : v8::Extension(kExtensionName, GetSource()) {}
  ~ExtensionImpl() {}

  virtual v8::Handle<v8::FunctionTemplate> GetNativeFunction(
      v8::Handle<v8::String> name) {
    if (name->Equals(v8::String::New("OpenChannelToExtension"))) {
      return v8::FunctionTemplate::New(OpenChannelToExtension);
    } else if (name->Equals(v8::String::New("PostMessage"))) {
      return v8::FunctionTemplate::New(PostMessage);
    } else if (name->Equals(v8::String::New("RegisterScriptAPI"))) {
      return v8::FunctionTemplate::New(RegisterScriptAPI);
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

  // This method is internal to the extension, and fulfills a dual purpose:
  // 1. Keep track of which v8::Contexts have registered event listeners.
  // 2. Registers a private variable on the context that we use for dispatching
  // events as they come in, by calling designated methods.
  static v8::Handle<v8::Value> RegisterScriptAPI(const v8::Arguments& args) {
    if (args.Length() >= 1 && args[0]->IsObject()) {
      v8::Persistent<v8::Context> context =
          v8::Persistent<v8::Context>::New(v8::Context::GetCurrent());
      GetRegisteredContexts().push_back(context);
      context.MakeWeak(NULL, WeakContextCallback);
      context->Global()->SetHiddenValue(v8::String::New(kScriptAPI), args[0]);
      DCHECK(args[0]->ToObject()->Get(v8::String::New("dispatchOnConnect"))->
             IsFunction());
      DCHECK(args[0]->ToObject()->Get(v8::String::New("dispatchOnMessage"))->
             IsFunction());
      return v8::Undefined();
    }
    return v8::Undefined();
  }

  // Calls the given chromiumPrivate method in each registered context.
  static void CallMethod(const std::string& method_name, int argc,
                         v8::Handle<v8::Value>* argv) {
    for (ContextList::iterator it = GetRegisteredContexts().begin();
         it != GetRegisteredContexts().end(); ++it) {
      DCHECK(!it->IsEmpty());
      v8::Context::Scope context_scope(*it);
      v8::Local<v8::Object> global = (*it)->Global();

      // Check if the window object is gone, which means this context's frame
      // has been unloaded.
      v8::Local<v8::Value> window = global->Get(v8::String::New("window"));
      if (!window->IsObject())
        continue;

      // Retrieve our hidden variable and call the method on it.
      v8::Local<v8::Value> script_api = global->GetHiddenValue(
          v8::String::New(kScriptAPI));
      if (!script_api->IsObject()) {
        NOTREACHED();
        continue;
      }

      v8::Handle<v8::Value> function_obj = script_api->ToObject()->Get(
          v8::String::New(method_name.c_str()));
      if (!function_obj->IsFunction()) {
        NOTREACHED();
        continue;
      }

      v8::Handle<v8::Function> function =
          v8::Handle<v8::Function>::Cast(function_obj);
      if (!function.IsEmpty())
        function->Call(v8::Object::New(), argc, argv);
    }
  }

  // Called when a registered context is garbage collected.
  static void WeakContextCallback(v8::Persistent<v8::Value> obj, void*) {
    ContextList::iterator it = std::find(GetRegisteredContexts().begin(),
                                         GetRegisteredContexts().end(), obj);
    if (it == GetRegisteredContexts().end()) {
      NOTREACHED();
      return;
    }

    it->Dispose();
    it->Clear();
    GetRegisteredContexts().erase(it);
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
  ExtensionImpl::CallMethod("dispatchOnConnect", arraysize(argv), argv);
}

void RendererExtensionBindings::HandleMessage(const std::string& message,
                                              int port_id) {
  v8::HandleScope handle_scope;
  v8::Handle<v8::Value> argv[2];
  argv[0] = v8::String::New(message.c_str());
  argv[1] = v8::Integer::New(port_id);
  ExtensionImpl::CallMethod("dispatchOnMessage", arraysize(argv), argv);
}

}  // namespace extensions_v8
