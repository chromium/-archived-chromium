// Copyright (c) 2008, Google Inc.
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

#include "v8_binding.h"
#include "v8_custom.h"
#include "v8_proxy.h"
#include "WorkerContextExecutionProxy.h"

#include "ExceptionCode.h"
#include "Frame.h"
#include "MessagePort.h"
#include "V8Document.h"
#include "V8HTMLDocument.h"
#include "V8ObjectEventListener.h"
#include "Worker.h"

namespace WebCore {

CALLBACK_FUNC_DECL(WorkerConstructor) {
  INC_STATS(L"DOM.Worker.Constructor");

  if (!WorkerContextExecutionProxy::IsWebWorkersEnabled()) {
    V8Proxy::ThrowError(V8Proxy::SYNTAX_ERROR, "Worker is not enabled.");
    return v8::Undefined();
  }

  if (!args.IsConstructCall()) {
    V8Proxy::ThrowError(V8Proxy::TYPE_ERROR,
        "DOM object constructor cannot be called as a function.");
    return v8::Undefined();
  }

  if (args.Length() == 0) {
    V8Proxy::ThrowError(V8Proxy::SYNTAX_ERROR, "Not enough arguments");
    return v8::Undefined();
  }

  v8::TryCatch try_catch;
  v8::Handle<v8::String> script_url = args[0]->ToString();
  if (try_catch.HasCaught()) {
    v8::ThrowException(try_catch.Exception());
    return v8::Undefined();
  }
  if (script_url.IsEmpty()) {
    return v8::Undefined();
  }

  // Get the document.
  Frame* frame = V8Proxy::retrieveFrame();
  if (!frame)
    return v8::Undefined();
  Document* document = frame->document();

  // Create the worker object.
  // Note: it's OK to let this RefPtr go out of scope because we also call
  // SetDOMWrapper(), which effectively holds a reference to obj.
  ExceptionCode ec = 0;
  RefPtr<Worker> obj = Worker::create(
      ToWebCoreString(script_url), document, ec);

  // Setup the standard wrapper object internal fields.
  v8::Handle<v8::Object> wrapper_object = args.Holder();
  V8Proxy::SetDOMWrapper(
      wrapper_object, V8ClassIndex::WORKER, obj.get());

  obj->ref();
  V8Proxy::SetJSWrapperForActiveDOMObject(
      obj.get(), v8::Persistent<v8::Object>::New(wrapper_object));

  return wrapper_object;
}

// TODO(mbelshe) - merge these with XHR's CreateHiddenXHRDependency

// Use an array to hold dependents. It works like a ref-counted scheme.
// A value can be added more than once to the xhr object.
static void CreateHiddenDependency(v8::Local<v8::Object> object,
                                   v8::Local<v8::Value> value) {
  ASSERT(V8Proxy::GetDOMWrapperType(object) ==
      V8ClassIndex::WORKER);
  v8::Local<v8::Value> cache =
      object->GetInternalField(V8Custom::kWorkerRequestCacheIndex);
  if (cache->IsNull() || cache->IsUndefined()) {
    cache = v8::Array::New();
    object->SetInternalField(V8Custom::kWorkerRequestCacheIndex, cache);
  }

  v8::Local<v8::Array> cache_array = v8::Local<v8::Array>::Cast(cache);
  cache_array->Set(v8::Integer::New(cache_array->Length()), value);
}

static void RemoveHiddenDependency(v8::Local<v8::Object> object,
                                   v8::Local<v8::Value> value) {
  ASSERT(V8Proxy::GetDOMWrapperType(object) == V8ClassIndex::WORKER);
  v8::Local<v8::Value> cache =
      object->GetInternalField(V8Custom::kWorkerRequestCacheIndex);
  ASSERT(cache->IsArray());
  v8::Local<v8::Array> cache_array = v8::Local<v8::Array>::Cast(cache);
  for (int i = cache_array->Length() - 1; i >= 0; i--) {
    v8::Local<v8::Value> cached = cache_array->Get(v8::Integer::New(i));
    if (cached->StrictEquals(value)) {
      cache_array->Delete(i);
      return;
    }
  }

  // We should only get here if we try to remove an event listener that was
  // never added.
}

ACCESSOR_GETTER(WorkerOnmessage) {
  INC_STATS(L"DOM.Worker.onmessage._get");
  Worker* imp = V8Proxy::ToNativeObject<Worker>(
      V8ClassIndex::WORKER, info.Holder());
  if (imp->onmessage()) {
    V8ObjectEventListener* listener =
        static_cast<V8ObjectEventListener*>(imp->onmessage());
    v8::Local<v8::Object> v8_listener = listener->getListenerObject();
    return v8_listener;
  }
  return v8::Undefined();
}

ACCESSOR_SETTER(WorkerOnmessage) {
  INC_STATS(L"DOM.Worker.onmessage._set");
  Worker* imp = V8Proxy::ToNativeObject<Worker>(
      V8ClassIndex::WORKER, info.Holder());
  V8ObjectEventListener* old_listener =
      static_cast<V8ObjectEventListener*>(imp->onmessage());
  if (value->IsNull()) {
    if (old_listener) {
      v8::Local<v8::Object> old_v8_listener = old_listener->getListenerObject();
      RemoveHiddenDependency(info.Holder(), old_v8_listener);
    }

    // Clear the listener
    imp->setOnmessage(0);

  } else {
    V8Proxy* proxy = V8Proxy::retrieve(imp->scriptExecutionContext());
    if (!proxy)
      return;

    RefPtr<EventListener> listener =
      proxy->FindOrCreateObjectEventListener(value, false);
    if (listener) {
      if (old_listener) {
        v8::Local<v8::Object> old_v8_listener =
            old_listener->getListenerObject();
        RemoveHiddenDependency(info.Holder(), old_v8_listener);
      }

      imp->setOnmessage(listener);
      CreateHiddenDependency(info.Holder(), value);
    }
  }
}

ACCESSOR_GETTER(WorkerOnerror) {
  INC_STATS(L"DOM.Worker.onerror._get");
  Worker* imp = V8Proxy::ToNativeObject<Worker>(
      V8ClassIndex::WORKER, info.Holder());
  if (imp->onerror()) {
    V8ObjectEventListener* listener =
        static_cast<V8ObjectEventListener*>(imp->onerror());
    v8::Local<v8::Object> v8_listener = listener->getListenerObject();
    return v8_listener;
  }
  return v8::Undefined();
}

ACCESSOR_SETTER(WorkerOnerror) {
  INC_STATS(L"DOM.Worker.onerror._set");
  Worker* imp = V8Proxy::ToNativeObject<Worker>(
      V8ClassIndex::WORKER, info.Holder());
  V8ObjectEventListener* old_listener =
      static_cast<V8ObjectEventListener*>(imp->onerror());
  if (value->IsNull()) {
    if (old_listener) {
      v8::Local<v8::Object> old_v8_listener =
          old_listener->getListenerObject();
      RemoveHiddenDependency(info.Holder(), old_v8_listener);
    }

    // Clear the listener
    imp->setOnerror(0);
  } else {
    V8Proxy* proxy = V8Proxy::retrieve(imp->scriptExecutionContext());
    if (!proxy)
      return;

    RefPtr<EventListener> listener =
      proxy->FindOrCreateObjectEventListener(value, false);
    if (listener) {
      if (old_listener) {
        v8::Local<v8::Object> old_v8_listener = old_listener->getListenerObject();
        RemoveHiddenDependency(info.Holder(), old_v8_listener);
      }

      imp->setOnerror(listener);
      CreateHiddenDependency(info.Holder(), value);
    }
  }
}

CALLBACK_FUNC_DECL(WorkerAddEventListener) {
  INC_STATS(L"DOM.Worker.addEventListener()");
  Worker* imp = V8Proxy::ToNativeObject<Worker>(
      V8ClassIndex::WORKER, args.Holder());

  V8Proxy* proxy = V8Proxy::retrieve(imp->scriptExecutionContext());
  if (!proxy)
    return v8::Undefined();

  RefPtr<EventListener> listener =
      proxy->FindOrCreateObjectEventListener(args[1], false);
  if (listener) {
    String type = ToWebCoreString(args[0]);
    bool useCapture = args[2]->BooleanValue();
    imp->addEventListener(type, listener, useCapture);

    CreateHiddenDependency(args.Holder(), args[1]);
  }
  return v8::Undefined();
}

CALLBACK_FUNC_DECL(WorkerRemoveEventListener) {
  INC_STATS(L"DOM.Worker.removeEventListener()");
  Worker* imp = V8Proxy::ToNativeObject<Worker>(
      V8ClassIndex::WORKER, args.Holder());

  V8Proxy* proxy = V8Proxy::retrieve(imp->scriptExecutionContext());
  if (!proxy)
    return v8::Undefined();  // probably leaked

  RefPtr<EventListener> listener =
    proxy->FindObjectEventListener(args[1], false);

  if (listener) {
    String type = ToWebCoreString(args[0]);
    bool useCapture = args[2]->BooleanValue();
    imp->removeEventListener(type, listener.get(), useCapture);

    RemoveHiddenDependency(args.Holder(), args[1]);
  }

  return v8::Undefined();
}

}  // namespace WebCore

#endif  // ENABLE(WORKERS)
