// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/extensions/event_bindings.h"

#include "base/basictypes.h"
#include "base/singleton.h"
#include "chrome/common/render_messages.h"
#include "chrome/renderer/extensions/bindings_utils.h"
#include "chrome/renderer/extensions/event_bindings.h"
#include "chrome/renderer/js_only_v8_extensions.h"
#include "chrome/renderer/render_thread.h"
#include "grit/renderer_resources.h"
#include "webkit/glue/webframe.h"

namespace {

// Keep a local cache of RenderThread so that we can mock it out for unit tests.
static RenderThreadBase* render_thread = NULL;

static RenderThreadBase* GetRenderThread() {
  return render_thread ? render_thread : RenderThread::current();
}

// Keep a list of contexts that have registered themselves with us.  This lets
// us know where to dispatch events when we receive them.
typedef std::list< v8::Persistent<v8::Context> > ContextList;
struct ExtensionData {
  ContextList contexts;
  std::map<std::string, int> listener_count;
};
ContextList& GetRegisteredContexts() {
  return Singleton<ExtensionData>::get()->contexts;
}
int EventIncrementListenerCount(const std::string& event_name) {
  ExtensionData *data = Singleton<ExtensionData>::get();
  return ++(data->listener_count[event_name]);
}
int EventDecrementListenerCount(const std::string& event_name) {
  ExtensionData *data = Singleton<ExtensionData>::get();
  return --(data->listener_count[event_name]);
}

const char* kContextAttachCount = "chromium.attachCount";

class ExtensionImpl : public v8::Extension {
 public:
  ExtensionImpl()
      : v8::Extension(EventBindings::kName,
                      GetStringResource<IDR_EVENT_BINDINGS_JS>(),
                      0, NULL) {
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
  static v8::Handle<v8::Value> AttachEvent(const v8::Arguments& args) {
    DCHECK(args.Length() == 1);
    // TODO(erikkay) should enforce that event name is a string in the bindings
    DCHECK(args[0]->IsString() || args[0]->IsUndefined());

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

    if (args[0]->IsString()) {
      std::string event_name(*v8::String::AsciiValue(args[0]));
      if (EventIncrementListenerCount(event_name) == 1) {
        GetRenderThread()->Send(
            new ViewHostMsg_ExtensionAddListener(event_name));
      }
    }

    return v8::Undefined();
  }

  static v8::Handle<v8::Value> DetachEvent(const v8::Arguments& args) {
    DCHECK(args.Length() == 1);
    // TODO(erikkay) should enforce that event name is a string in the bindings
    DCHECK(args[0]->IsString() || args[0]->IsUndefined());

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

    if (args[0]->IsString()) {
      std::string event_name(*v8::String::AsciiValue(args[0]));
      if (EventDecrementListenerCount(event_name) == 0) {
        GetRenderThread()->Send(
          new ViewHostMsg_ExtensionRemoveListener(event_name));
      }
    }

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

// static
void EventBindings::SetRenderThread(RenderThreadBase* thread) {
  render_thread = thread;
}

// static
void EventBindings::HandleContextCreated(WebFrame* frame) {
  v8::HandleScope handle_scope;
  v8::Local<v8::Context> context = frame->GetScriptContext();
  DCHECK(!context.IsEmpty());
  // TODO(mpcomplete): register it
}

// static
void EventBindings::HandleContextDestroyed(WebFrame* frame) {
  v8::HandleScope handle_scope;
  v8::Local<v8::Context> context = frame->GetScriptContext();
  DCHECK(!context.IsEmpty());
  // TODO(mpcomplete): unregister it, dispatch event
}

void EventBindings::CallFunction(const std::string& function_name,
                                 int argc, v8::Handle<v8::Value>* argv) {
  for (ContextList::iterator it = GetRegisteredContexts().begin();
       it != GetRegisteredContexts().end(); ++it) {
    CallFunctionInContext(*it, function_name, argc, argv);
  }
}
