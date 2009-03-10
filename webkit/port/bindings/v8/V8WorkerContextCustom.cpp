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

#include "v8_binding.h"
#include "v8_custom.h"
#include "v8_events.h"
#include "v8_proxy.h"
#include "WorkerContextExecutionProxy.h"

#include "V8Document.h"
#include "V8HTMLDocument.h"

#include "ExceptionCode.h"
#include "MessagePort.h"
#include "NotImplemented.h"
#include "WorkerContext.h"

namespace WebCore {

// TODO(mbelshe) - merge these with XHR's CreateHiddenXHRDependency

// Use an array to hold dependents. It works like a ref-counted scheme.
// A value can be added more than once to the xhr object.
static void CreateHiddenDependency(v8::Local<v8::Object> object,
                                   v8::Local<v8::Value> value) {
  ASSERT(V8Proxy::GetDOMWrapperType(object) == V8ClassIndex::WORKERCONTEXT);
  v8::Local<v8::Value> cache =
      object->GetInternalField(V8Custom::kWorkerContextRequestCacheIndex);
  if (cache->IsNull() || cache->IsUndefined()) {
    cache = v8::Array::New();
    object->SetInternalField(V8Custom::kWorkerContextRequestCacheIndex, cache);
  }

  v8::Local<v8::Array> cache_array = v8::Local<v8::Array>::Cast(cache);
  cache_array->Set(v8::Integer::New(cache_array->Length()), value);
}

static void RemoveHiddenDependency(v8::Local<v8::Object> object,
                                   v8::Local<v8::Value> value) {
  ASSERT(V8Proxy::GetDOMWrapperType(object) == V8ClassIndex::WORKERCONTEXT);
  v8::Local<v8::Value> cache =
      object->GetInternalField(V8Custom::kWorkerContextRequestCacheIndex);
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

ACCESSOR_GETTER(WorkerContextSelf) {
  INC_STATS(L"DOM.WorkerContext.self._get");
  WorkerContext* imp = V8Proxy::ToNativeObject<WorkerContext>(
      V8ClassIndex::WORKERCONTEXT, info.Holder());
  return V8Proxy::ToV8Object(V8ClassIndex::WORKERCONTEXT, imp);
}

ACCESSOR_GETTER(WorkerContextOnmessage) {
  INC_STATS(L"DOM.WorkerContext.onmessage._get");
  WorkerContext* imp = V8Proxy::ToNativeObject<WorkerContext>(
      V8ClassIndex::WORKERCONTEXT, info.Holder());
  if (imp->onmessage()) {
    V8WorkerContextEventListener* listener =
        static_cast<V8WorkerContextEventListener*>(imp->onmessage());
    v8::Local<v8::Object> v8_listener = listener->GetListenerObject();
    return v8_listener;
  }
  return v8::Undefined();
}

ACCESSOR_SETTER(WorkerContextOnmessage) {
  INC_STATS(L"DOM.WorkerContext.onmessage._set");
  WorkerContext* imp = V8Proxy::ToNativeObject<WorkerContext>(
      V8ClassIndex::WORKERCONTEXT, info.Holder());
  V8WorkerContextEventListener* old_listener =
      static_cast<V8WorkerContextEventListener*>(imp->onmessage());
  if (value->IsNull()) {
    if (imp->onmessage()) {
      v8::Local<v8::Object> old_v8_listener = old_listener->GetListenerObject();
      RemoveHiddenDependency(info.Holder(), old_v8_listener);
    }

    // Clear the listener
    imp->setOnmessage(0);

  } else {
    RefPtr<V8EventListener> listener =
        imp->script()->proxy()->FindOrCreateEventListener(
            v8::Local<v8::Object>::Cast(value), false, false);
    if (listener) {
      if (old_listener) {
        v8::Local<v8::Object> old_v8_listener =
            old_listener->GetListenerObject();
        RemoveHiddenDependency(info.Holder(), old_v8_listener);
      }

      imp->setOnmessage(listener);
      CreateHiddenDependency(info.Holder(), value);
    }
  }
}

v8::Handle<v8::Value> SetTimeoutOrInterval(const v8::Arguments& args,
                                           bool singleShot) {
  WorkerContext* imp = V8Proxy::ToNativeObject<WorkerContext>(
      V8ClassIndex::WORKERCONTEXT, args.Holder());

  int delay = ToInt32(args[1]);

  notImplemented();

  return v8::Undefined();
}

v8::Handle<v8::Value> ClearTimeoutOrInterval(const v8::Arguments& args) {
  WorkerContext* imp = V8Proxy::ToNativeObject<WorkerContext>(
      V8ClassIndex::WORKERCONTEXT, args.Holder());

  bool ok = false;
  int tid = ToInt32(args[0], ok);
  if (ok) {
    imp->removeTimeout(tid);
  }

  return v8::Undefined();
}

CALLBACK_FUNC_DECL(WorkerContextSetTimeout) {
  INC_STATS(L"DOM.WorkerContext.setTimeout()");
  return SetTimeoutOrInterval(args, true);
}

CALLBACK_FUNC_DECL(WorkerContextClearTimeout) {
  INC_STATS(L"DOM.WorkerContext.clearTimeout()");
  return ClearTimeoutOrInterval(args);
}

CALLBACK_FUNC_DECL(WorkerContextSetInterval) {
  INC_STATS(L"DOM.WorkerContext.setInterval()");
  return SetTimeoutOrInterval(args, false);
}

CALLBACK_FUNC_DECL(WorkerContextClearInterval) {
  INC_STATS(L"DOM.WorkerContext.clearInterval()");
  return ClearTimeoutOrInterval(args);
}

CALLBACK_FUNC_DECL(WorkerContextAddEventListener) {
  INC_STATS(L"DOM.WorkerContext.addEventListener()");
  WorkerContext* imp = V8Proxy::ToNativeObject<WorkerContext>(
      V8ClassIndex::WORKERCONTEXT, args.Holder());

  RefPtr<V8EventListener> listener =
      imp->script()->proxy()->FindOrCreateEventListener(
          v8::Local<v8::Object>::Cast(args[1]), false, false);

  if (listener) {
    String type = ToWebCoreString(args[0]);
    bool useCapture = args[2]->BooleanValue();
    imp->addEventListener(type, listener, useCapture);

    CreateHiddenDependency(args.Holder(), args[1]);
  }
  return v8::Undefined();
}

CALLBACK_FUNC_DECL(WorkerContextRemoveEventListener) {
  INC_STATS(L"DOM.WorkerContext.removeEventListener()");
  WorkerContext* imp = V8Proxy::ToNativeObject<WorkerContext>(
      V8ClassIndex::WORKERCONTEXT, args.Holder());
  WorkerContextExecutionProxy* proxy = imp->script()->proxy();

  RefPtr<V8EventListener> listener = proxy->FindOrCreateEventListener(
      v8::Local<v8::Object>::Cast(args[1]), false, true);

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
