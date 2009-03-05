// Copyright (c) 2009, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "config.h"

#if ENABLE(WORKERS)

#include "WorkerContextExecutionProxy.h"

#include "v8_binding.h"
#include "v8_events.h"
#include "v8_index.h"
#include "v8_proxy.h"
#include "WorkerContext.h"
#include "WorkerLocation.h"
#include "WorkerNavigator.h"
#include "WorkerScriptController.h"

namespace WebCore {

WorkerContextExecutionProxy::WorkerContextExecutionProxy(
    WorkerContext* workerContext)
  : m_workerContext(workerContext),
    m_recursion(0) {
}

WorkerContextExecutionProxy::~WorkerContextExecutionProxy() {
  Dispose();
}

void WorkerContextExecutionProxy::Dispose() {
  // Disconnect all event listeners.
  for (EventListenerList::iterator iter = m_listeners.begin();
       iter != m_listeners.end();
       ++iter) {
    static_cast<V8WorkerContextEventListener*>(*iter)->disconnect();
  }
  m_listeners.clear();

  // Detach all events from their JS wrappers.
  for (EventSet::iterator iter = m_events.begin();
       iter != m_events.end();
       ++iter) {
    Event* event = *iter;
    if (ForgetV8EventObject(event)) {
      event->deref();
    }
  }
  m_events.clear();

  // Dispose the context.
  if (!m_context.IsEmpty()) {
    m_context.Dispose();
    m_context.Clear();
  }
}

WorkerContextExecutionProxy* WorkerContextExecutionProxy::retrieve() {
  v8::Handle<v8::Context> context = v8::Context::GetCurrent();
  v8::Handle<v8::Object> global = context->Global();
  global = V8Proxy::LookupDOMWrapper(V8ClassIndex::WORKERCONTEXT, global);
  ASSERT(!global.IsEmpty());
  WorkerContext* worker_context =
      V8Proxy::ToNativeObject<WorkerContext>(V8ClassIndex::WORKERCONTEXT,
                                             global);
  return worker_context->script()->proxy();
}

void WorkerContextExecutionProxy::InitContextIfNeeded() {
  // Bail out if the context has already been initialized.
  if (!m_context.IsEmpty()) {
    return;
  }

  // Create a new environment
  v8::Persistent<v8::ObjectTemplate> global_template;
  m_context = v8::Context::New(NULL, global_template);

  // Starting from now, use local context only.
  v8::Local<v8::Context> context = v8::Local<v8::Context>::New(m_context);
  v8::Context::Scope scope(context);

  // Allocate strings used during initialization.
  v8::Handle<v8::String> implicit_proto_string = v8::String::New("__proto__");

  // Create a new JS object and use it as the prototype for the
  // shadow global object.
  v8::Handle<v8::Function> worker_context_constructor =
      GetConstructor(V8ClassIndex::WORKERCONTEXT);
  v8::Local<v8::Object> js_worker_context =
      SafeAllocation::NewInstance(worker_context_constructor);
  // Bail out if allocation failed.
  if (js_worker_context.IsEmpty()) {
    Dispose();
    return;
  }

  // Wrap the object.
  V8Proxy::SetDOMWrapper(js_worker_context,
                         V8ClassIndex::ToInt(V8ClassIndex::WORKERCONTEXT),
                         m_workerContext);

  V8Proxy::SetJSWrapperForDOMObject(m_workerContext,
      v8::Persistent<v8::Object>::New(js_worker_context));

  // Insert the object instance as the prototype of the shadow object.
  v8::Handle<v8::Object> v8_global = m_context->Global();
  v8_global->Set(implicit_proto_string, js_worker_context);
}

v8::Local<v8::Function> WorkerContextExecutionProxy::GetConstructor(
    V8ClassIndex::V8WrapperType t) {
  // Enter the context of the proxy to make sure that the
  // function is constructed in the context corresponding to
  // this proxy.
  v8::Context::Scope scope(m_context);
  v8::Handle<v8::FunctionTemplate> templ = V8Proxy::GetTemplate(t);

  // Getting the function might fail if we're running out of
  // stack or memory.
  v8::TryCatch try_catch;
  v8::Local<v8::Function> value = templ->GetFunction();
  if (value.IsEmpty()) {
    return v8::Local<v8::Function>();
  }

  return value;
}

v8::Handle<v8::Value> WorkerContextExecutionProxy::ToV8Object(
    V8ClassIndex::V8WrapperType type, void* imp) {
  if (!imp) return v8::Null();

  // Non DOM node
  v8::Persistent<v8::Object> result = GetDOMObjectMap().get(imp);
  if (result.IsEmpty()) {
    v8::Local<v8::Object> v8obj = InstantiateV8Object(type, type, imp);
    if (!v8obj.IsEmpty()) {
      // Go through big switch statement, it has some duplications
      // that were handled by code above (such as CSSVALUE, CSSRULE, etc).
      switch (type) {
        case V8ClassIndex::WORKERLOCATION:
          static_cast<WorkerLocation*>(imp)->ref();
          break;
        case V8ClassIndex::WORKERNAVIGATOR:
          static_cast<WorkerNavigator*>(imp)->ref();
          break;
        default:
          ASSERT(false);
      }
      result = v8::Persistent<v8::Object>::New(v8obj);
      V8Proxy::SetJSWrapperForDOMObject(imp, result);
    }
  }
  return result;
}

v8::Handle<v8::Value> WorkerContextExecutionProxy::EventToV8Object(
    Event* event) {
  if (!event)
    return v8::Null();

  v8::Handle<v8::Object> wrapper = GetDOMObjectMap().get(event);
  if (!wrapper.IsEmpty())
    return wrapper;

  V8ClassIndex::V8WrapperType type = V8ClassIndex::EVENT;

  if (event->isMessageEvent())
    type = V8ClassIndex::MESSAGEEVENT;

  v8::Handle<v8::Object> result =
      InstantiateV8Object(type, V8ClassIndex::EVENT, event);
  if (result.IsEmpty()) {
    // Instantiation failed. Avoid updating the DOM object map and
    // return null which is already handled by callers of this function
    // in case the event is NULL.
    return v8::Null();
  }

  event->ref();  // fast ref
  V8Proxy::SetJSWrapperForDOMObject(
      event, v8::Persistent<v8::Object>::New(result));

  return result;
}

// A JS object of type EventTarget in the worker context can only be
// WorkerContext.
v8::Handle<v8::Value> WorkerContextExecutionProxy::EventTargetToV8Object(
    EventTarget* target) {
  if (!target)
    return v8::Null();

  WorkerContext* worker_context = target->toWorkerContext();
  if (worker_context)
    return WorkerContextToV8Object(worker_context);

  ASSERT(0);
  return v8::Handle<v8::Value>();
}

v8::Handle<v8::Value> WorkerContextExecutionProxy::WorkerContextToV8Object(
    WorkerContext* worker_context) {
  if (!worker_context) return v8::Null();

  v8::Handle<v8::Context> context =
      worker_context->script()->proxy()->GetContext();

  v8::Handle<v8::Object> global = context->Global();
  ASSERT(!global.IsEmpty());
  return global;
}

v8::Local<v8::Object> WorkerContextExecutionProxy::InstantiateV8Object(
    V8ClassIndex::V8WrapperType desc_type,
    V8ClassIndex::V8WrapperType cptr_type,
    void* imp) {
  v8::Local<v8::Function> function;
  WorkerContextExecutionProxy* proxy = retrieve();
  if (proxy) {
    function = proxy->GetConstructor(desc_type);
  } else {
    function = V8Proxy::GetTemplate(desc_type)->GetFunction();
  }
  v8::Local<v8::Object> instance = SafeAllocation::NewInstance(function);
  if (!instance.IsEmpty()) {
    // Avoid setting the DOM wrapper for failed allocations.
    V8Proxy::SetDOMWrapper(instance, V8ClassIndex::ToInt(cptr_type), imp);
  }
  return instance;
}

bool WorkerContextExecutionProxy::ForgetV8EventObject(Event* event) {
  if (GetDOMObjectMap().contains(event)) {
    GetDOMObjectMap().forget(event);
    return true;
  } else {
    return false;
  }
}

v8::Local<v8::Value> WorkerContextExecutionProxy::Evaluate(
    const String& str, const String& file_name, int base_line) {
  v8::HandleScope hs;

  InitContextIfNeeded();
  v8::Context::Scope scope(m_context);

  v8::Local<v8::String> code = v8ExternalString(str);
  v8::Handle<v8::Script> script = V8Proxy::CompileScript(code,
                                                         file_name,
                                                         base_line);
  return RunScript(script);
}

v8::Local<v8::Value> WorkerContextExecutionProxy::RunScript(
    v8::Handle<v8::Script> script) {
  if (script.IsEmpty())
    return v8::Local<v8::Value>();

  // Compute the source string and prevent against infinite recursion.
  if (m_recursion >= kMaxRecursionDepth) {
    v8::Local<v8::String> code =
        v8ExternalString("throw RangeError('Recursion too deep')");
    script = V8Proxy::CompileScript(code, "", 0);
  }

  if (V8Proxy::HandleOutOfMemory())
    ASSERT(script.IsEmpty());

  if (script.IsEmpty())
    return v8::Local<v8::Value>();

  // Run the script and keep track of the current recursion depth.
  v8::Local<v8::Value> result;
  {
    m_recursion++;
    result = script->Run();
    m_recursion--;
  }

  // Handle V8 internal error situation (Out-of-memory).
  if (result.IsEmpty())
    return v8::Local<v8::Value>();

  return result;
}

PassRefPtr<V8EventListener>
WorkerContextExecutionProxy::FindOrCreateEventListener(
    v8::Local<v8::Value> obj, bool is_inline, bool find_only) {
  if (!obj->IsObject())
    return 0;

  for (EventListenerList::iterator iter = m_listeners.begin();
       iter != m_listeners.end();
       ++iter) {
    V8EventListener* el = *iter;
    if (el->isInline() == is_inline && el->GetListenerObject() == obj)
      return el;
  }
  if (find_only)
    return NULL;

  // Create a new one, and add to cache.
  RefPtr<V8EventListener> listener = V8WorkerContextEventListener::create(
      this, v8::Local<v8::Object>::Cast(obj), is_inline);
  m_listeners.push_back(listener.get());

  return listener.release();
}

void WorkerContextExecutionProxy::RemoveEventListener(
    V8EventListener* listener) {
  for (EventListenerList::iterator iter = m_listeners.begin();
       iter != m_listeners.end();
       ++iter) {
    if (*iter == listener) {
      m_listeners.erase(iter);
      return;
    }
  }
}

void WorkerContextExecutionProxy::TrackEvent(Event* event) {
  m_events.add(event);
}

} // namespace WebCore

#endif // ENABLE(WORKERS)
