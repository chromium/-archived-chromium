// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/extensions/event_bindings.h"

#include "base/basictypes.h"
#include "base/singleton.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/url_constants.h"
#include "chrome/renderer/extensions/bindings_utils.h"
#include "chrome/renderer/extensions/event_bindings.h"
#include "chrome/renderer/js_only_v8_extensions.h"
#include "chrome/renderer/render_thread.h"
#include "chrome/renderer/render_view.h"
#include "grit/renderer_resources.h"
#include "webkit/glue/webframe.h"

using bindings_utils::CallFunctionInContext;
using bindings_utils::ContextInfo;
using bindings_utils::ContextList;
using bindings_utils::GetContexts;
using bindings_utils::GetStringResource;
using bindings_utils::ExtensionBase;
using bindings_utils::GetPendingRequestMap;
using bindings_utils::PendingRequest;
using bindings_utils::PendingRequestMap;

namespace {

// Keep a local cache of RenderThread so that we can mock it out for unit tests.
static RenderThreadBase* render_thread = NULL;

// Set to true if these bindings are registered.  Will be false when extensions
// are disabled.
static bool bindings_registered = false;

struct ExtensionData {
  std::map<std::string, int> listener_count;
};
int EventIncrementListenerCount(const std::string& event_name) {
  ExtensionData *data = Singleton<ExtensionData>::get();
  return ++(data->listener_count[event_name]);
}
int EventDecrementListenerCount(const std::string& event_name) {
  ExtensionData *data = Singleton<ExtensionData>::get();
  return --(data->listener_count[event_name]);
}

class ExtensionImpl : public ExtensionBase {
 public:
  ExtensionImpl()
      : ExtensionBase(EventBindings::kName,
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
    } else if (name->Equals(v8::String::New("GetNextRequestId"))) {
      return v8::FunctionTemplate::New(GetNextRequestId);
    }
    return ExtensionBase::GetNativeFunction(name);
  }

  // Attach an event name to an object.
  static v8::Handle<v8::Value> AttachEvent(const v8::Arguments& args) {
    DCHECK(args.Length() == 1);
    // TODO(erikkay) should enforce that event name is a string in the bindings
    DCHECK(args[0]->IsString() || args[0]->IsUndefined());

    if (args[0]->IsString()) {
      std::string event_name(*v8::String::AsciiValue(args[0]));
      if (EventIncrementListenerCount(event_name) == 1) {
        EventBindings::GetRenderThread()->Send(
            new ViewHostMsg_ExtensionAddListener(event_name));
      }
    }

    return v8::Undefined();
  }

  static v8::Handle<v8::Value> DetachEvent(const v8::Arguments& args) {
    DCHECK(args.Length() == 1);
    // TODO(erikkay) should enforce that event name is a string in the bindings
    DCHECK(args[0]->IsString() || args[0]->IsUndefined());

    if (args[0]->IsString()) {
      std::string event_name(*v8::String::AsciiValue(args[0]));
      if (EventDecrementListenerCount(event_name) == 0) {
        EventBindings::GetRenderThread()->Send(
          new ViewHostMsg_ExtensionRemoveListener(event_name));
      }
    }

    return v8::Undefined();
  }

  static v8::Handle<v8::Value> GetNextRequestId(const v8::Arguments& args) {
    static int next_request_id = 0;
    return v8::Integer::New(next_request_id++);
  }
};

}  // namespace

const char* EventBindings::kName = "chrome/EventBindings";

v8::Extension* EventBindings::Get() {
  bindings_registered = true;
  return new ExtensionImpl();
}

// static
void EventBindings::SetRenderThread(RenderThreadBase* thread) {
  render_thread = thread;
}

// static
RenderThreadBase* EventBindings::GetRenderThread() {
  return render_thread ? render_thread : RenderThread::current();
}

void EventBindings::HandleContextCreated(WebFrame* frame) {
  if (!bindings_registered)
    return;

  v8::HandleScope handle_scope;
  v8::Local<v8::Context> context = frame->GetScriptContext();
  DCHECK(!context.IsEmpty());
  DCHECK(bindings_utils::FindContext(context) == GetContexts().end());

  GURL url = frame->GetView()->GetMainFrame()->GetURL();
  std::string extension_id;
  if (url.SchemeIs(chrome::kExtensionScheme))
    extension_id = url.host();

  v8::Persistent<v8::Context> persistent_context =
      v8::Persistent<v8::Context>::New(context);
  GetContexts().push_back(linked_ptr<ContextInfo>(
      new ContextInfo(persistent_context, extension_id)));
}

// static
void EventBindings::HandleContextDestroyed(WebFrame* frame) {
  if (!bindings_registered)
    return;

  v8::HandleScope handle_scope;
  v8::Local<v8::Context> context = frame->GetScriptContext();
  DCHECK(!context.IsEmpty());

  ContextList::iterator it = bindings_utils::FindContext(context);
  DCHECK(it != GetContexts().end());

  // Notify the bindings that they're going away.
  CallFunctionInContext(context, "dispatchOnUnload", 0, NULL);

  // Remove all pending requests for this context.
  PendingRequestMap& pending_requests = GetPendingRequestMap();
  for (PendingRequestMap::iterator it = pending_requests.begin();
       it != pending_requests.end(); ) {
    PendingRequestMap::iterator current = it++;
    if (current->second->context == context) {
      current->second->context.Dispose();
      current->second->context.Clear();
      pending_requests.erase(current);
    }
  }

  // Remove it from our registered contexts.
  (*it)->context.Dispose();
  (*it)->context.Clear();
  GetContexts().erase(it);
}

// static
void EventBindings::CallFunction(const std::string& function_name,
                                 int argc, v8::Handle<v8::Value>* argv) {
  v8::HandleScope handle_scope;
  for (ContextList::iterator it = GetContexts().begin();
       it != GetContexts().end(); ++it) {
    CallFunctionInContext((*it)->context, function_name, argc, argv);
  }
}

// static
void EventBindings::HandleResponse(int request_id, bool success,
                                   const std::string& response,
                                   const std::string& error) {
  PendingRequest* request = GetPendingRequestMap()[request_id].get();
  if (!request)
    return;  // The frame went away.

  v8::HandleScope handle_scope;
  v8::Handle<v8::Value> argv[5];
  argv[0] = v8::Integer::New(request_id);
  argv[1] = v8::String::New(request->name.c_str());
  argv[2] = v8::Boolean::New(success);
  argv[3] = v8::String::New(response.c_str());
  argv[4] = v8::String::New(error.c_str());
  CallFunctionInContext(
      request->context, "handleResponse", arraysize(argv), argv);

  GetPendingRequestMap().erase(request_id);
}
