// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/extensions/event_bindings.h"

#include "base/basictypes.h"
#include "base/singleton.h"
#include "chrome/renderer/extensions/bindings_utils.h"
#include "chrome/renderer/extensions/event_bindings.h"
#include "chrome/renderer/js_only_v8_extensions.h"
#include "chrome/renderer/render_thread.h"
#include "grit/renderer_resources.h"

namespace {

// Keep a list of contexts that have registered themselves with us.  This lets
// us know where to dispatch events when we receive them.
typedef std::list< v8::Persistent<v8::Context> > ContextList;
struct ExtensionData {
  ContextList contexts;
};
ContextList& GetRegisteredContexts() {
  return Singleton<ExtensionData>::get()->contexts;
}

const char* kExtensionDeps[] = { JsonJsV8Extension::kName };
const char* kContextAttachCount = "chromium.attachCount";

class ExtensionImpl : public v8::Extension {
 public:
  ExtensionImpl()
      : v8::Extension(EventBindings::kName,
                      GetStringResource<IDR_EVENT_BINDINGS_JS>(),
                      arraysize(kExtensionDeps), kExtensionDeps) {
  }
  ~ExtensionImpl() {}

  virtual v8::Handle<v8::FunctionTemplate> GetNativeFunction(
      v8::Handle<v8::String> name) {
    if (name->Equals(v8::String::New("AttachEvent"))) {
      return v8::FunctionTemplate::New(AttachEvent);
    } else if (name->Equals(v8::String::New("DetachEvent"))) {
      return v8::FunctionTemplate::New(DetachEvent);
    }
    return v8::Handle<v8::FunctionTemplate>();
  }

  // Attach an event name to an object.
  // TODO(mpcomplete): I'm just using this to register the v8 Context right now.
  // The idea is to eventually notify the browser about what events are being
  // listened to, so it can dispatch appropriately.
  static v8::Handle<v8::Value> AttachEvent(const v8::Arguments& args) {
    v8::Persistent<v8::Context> context =
        v8::Persistent<v8::Context>::New(v8::Context::GetCurrent());
    v8::Local<v8::Object> global = context->Global();

    // Remember how many times this context has been attached, so we can
    // register the context on first attach and unregister on last detach.
    v8::Local<v8::Value> attach_count = global->GetHiddenValue(
        v8::String::New(kContextAttachCount));
    int32_t account_count_value =
        (!attach_count.IsEmpty() && attach_count->IsNumber()) ?
        attach_count->Int32Value() : 0;
    if (account_count_value == 0) {
      // First time attaching.
      GetRegisteredContexts().push_back(context);
      context.MakeWeak(NULL, WeakContextCallback);
    }
    global->SetHiddenValue(
        v8::String::New(kContextAttachCount),
        v8::Integer::New(account_count_value + 1));

    return v8::Undefined();
  }

  static v8::Handle<v8::Value> DetachEvent(const v8::Arguments& args) {
    v8::Local<v8::Context> context = v8::Context::GetCurrent();
    v8::Local<v8::Object> global = context->Global();
    v8::Local<v8::Value> attach_count = global->GetHiddenValue(
        v8::String::New(kContextAttachCount));
    DCHECK(!attach_count.IsEmpty() && attach_count->IsNumber());
    int32_t account_count_value = attach_count->Int32Value();
    DCHECK(account_count_value > 0);
    if (account_count_value == 1) {
      // Clean up after last detach.
      UnregisterContext(context);
    }
    global->SetHiddenValue(
        v8::String::New(kContextAttachCount),
        v8::Integer::New(account_count_value - 1));

    return v8::Undefined();
  }

  // Called when a registered context is garbage collected.
  static void UnregisterContext(v8::Handle<void> context) {
    ContextList& contexts = GetRegisteredContexts();
    ContextList::iterator it = std::find(contexts.begin(), contexts.end(),
                                         context);
    if (it == contexts.end()) {
      NOTREACHED();
      return;
    }

    it->Dispose();
    it->Clear();
    contexts.erase(it);
  }

  // Called when a registered context is garbage collected.
  static void WeakContextCallback(v8::Persistent<v8::Value> obj, void*) {
    UnregisterContext(obj);
  }
};

}  // namespace

const char* EventBindings::kName = "chrome/EventBindings";

v8::Extension* EventBindings::Get() {
  return new ExtensionImpl();
}

void EventBindings::CallFunction(const std::string& function_name,
                                 int argc, v8::Handle<v8::Value>* argv) {
  for (ContextList::iterator it = GetRegisteredContexts().begin();
       it != GetRegisteredContexts().end(); ++it) {
    DCHECK(!it->IsEmpty());
    v8::Context::Scope context_scope(*it);
    v8::Local<v8::Object> global = (*it)->Global();

    v8::Local<v8::Script> script = v8::Script::Compile(
        v8::String::New(function_name.c_str()));
    v8::Local<v8::Value> function_obj = script->Run();
    if (!function_obj->IsFunction())
      continue;

    v8::Local<v8::Function> function =
        v8::Local<v8::Function>::Cast(function_obj);
    if (!function.IsEmpty())
      function->Call(v8::Object::New(), argc, argv);
  }
}
