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

#include "v8_binding.h"
#include "v8_custom.h"
#include "v8_events.h"
#include "v8_proxy.h"

#include "V8Document.h"
#include "V8HTMLDocument.h"

#include "ExceptionCode.h"
#include "MessagePort.h"

namespace WebCore {

// TODO(mbelshe) - merge these with XHR's CreateHiddenXHRDependency

// Use an array to hold dependents. It works like a ref-counted scheme.
// A value can be added more than once to the xhr object.
static void CreateHiddenDependency(v8::Local<v8::Object> object,
                                   v8::Local<v8::Value> value) {
  ASSERT(V8Proxy::GetDOMWrapperType(object) ==
      V8ClassIndex::MESSAGEPORT);
  v8::Local<v8::Value> cache =
      object->GetInternalField(V8Custom::kMessagePortRequestCacheIndex);
  if (cache->IsNull() || cache->IsUndefined()) {
    cache = v8::Array::New();
    object->SetInternalField(V8Custom::kXMLHttpRequestCacheIndex, cache);
  }

  v8::Local<v8::Array> cache_array = v8::Local<v8::Array>::Cast(cache);
  cache_array->Set(v8::Integer::New(cache_array->Length()), value);
}

static void RemoveHiddenDependency(v8::Local<v8::Object> object,
                                   v8::Local<v8::Value> value) {
  ASSERT(V8Proxy::GetDOMWrapperType(object) == V8ClassIndex::MESSAGEPORT);
  v8::Local<v8::Value> cache =
      object->GetInternalField(V8Custom::kMessagePortRequestCacheIndex);
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

ACCESSOR_GETTER(MessagePortOnmessage) {
  INC_STATS(L"DOM.MessagePort.onmessage._get");
  MessagePort* imp = V8Proxy::ToNativeObject<MessagePort>(
      V8ClassIndex::MESSAGEPORT, info.Holder());
  if (imp->onmessage()) {
    V8ObjectEventListener* listener =
        static_cast<V8ObjectEventListener*>(imp->onmessage());
    v8::Local<v8::Object> v8_listener = listener->GetListenerObject();
    return v8_listener;
  }
  return v8::Undefined();
}

ACCESSOR_SETTER(MessagePortOnmessage) {
  INC_STATS(L"DOM.MessagePort.onmessage._set");
  MessagePort* imp = V8Proxy::ToNativeObject<MessagePort>(
      V8ClassIndex::MESSAGEPORT, info.Holder());
  if (value->IsNull()) {
    if (imp->onmessage()) {
      V8ObjectEventListener* listener =
          static_cast<V8ObjectEventListener*>(imp->onmessage());
      v8::Local<v8::Object> v8_listener = listener->GetListenerObject();
      RemoveHiddenDependency(info.Holder(), v8_listener);
    }

    // Clear the listener
    imp->setOnmessage(0);

  } else {
    V8Proxy* proxy = V8Proxy::retrieve(imp->document()->frame());
    if (!proxy)
      return;

    RefPtr<EventListener> listener =
      proxy->FindOrCreateObjectEventListener(value, false);
    if (listener) {
      imp->setOnmessage(listener);
      CreateHiddenDependency(info.Holder(), value);
    }
  }
}

ACCESSOR_GETTER(MessagePortOnclose) {
  INC_STATS(L"DOM.MessagePort.onclose._get");
  MessagePort* imp = V8Proxy::ToNativeObject<MessagePort>(
      V8ClassIndex::MESSAGEPORT, info.Holder());
  if (imp->onclose()) {
    V8ObjectEventListener* listener =
        static_cast<V8ObjectEventListener*>(imp->onclose());
    v8::Local<v8::Object> v8_listener = listener->GetListenerObject();
    return v8_listener;
  }
  return v8::Undefined();
}

ACCESSOR_SETTER(MessagePortOnclose) {
  INC_STATS(L"DOM.MessagePort.onclose._set");
  MessagePort* imp = V8Proxy::ToNativeObject<MessagePort>(
      V8ClassIndex::MESSAGEPORT, info.Holder());
  if (value->IsNull()) {
    if (imp->onclose()) {
      V8ObjectEventListener* listener =
          static_cast<V8ObjectEventListener*>(imp->onclose());
      v8::Local<v8::Object> v8_listener = listener->GetListenerObject();
      RemoveHiddenDependency(info.Holder(), v8_listener);
    }

    // Clear the listener
    imp->setOnclose(0);
  } else {
    V8Proxy* proxy = V8Proxy::retrieve(imp->document()->frame());
    if (!proxy)
      return;

    RefPtr<EventListener> listener =
      proxy->FindOrCreateObjectEventListener(value, false);
    if (listener) {
      imp->setOnclose(listener);
      CreateHiddenDependency(info.Holder(), value);
    }
  }
}

CALLBACK_FUNC_DECL(MessagePortStartConversation) {
  INC_STATS(L"DOM.MessagePort.StartConversation()");
  if (args.Length() < 1) {
    V8Proxy::ThrowError(V8Proxy::SYNTAX_ERROR, "Not enough arguments");
    return v8::Undefined();
  }

  MessagePort* imp = V8Proxy::ToNativeObject<MessagePort>(
      V8ClassIndex::MESSAGEPORT, args.Holder());

  V8Proxy* proxy = V8Proxy::retrieve(imp->document()->frame());
  if (!proxy)
    return v8::Undefined();

  RefPtr<MessagePort> port = imp->startConversation(imp->document(),
      ToWebCoreString(args[0]));
  v8::Handle<v8::Value> wrapper = 
    V8Proxy::ToV8Object(V8ClassIndex::MESSAGEPORT, port.get());
  return wrapper;
}

CALLBACK_FUNC_DECL(MessagePortAddEventListener) {
  INC_STATS(L"DOM.MessagePort.AddEventListener()");
  MessagePort* imp = V8Proxy::ToNativeObject<MessagePort>(
      V8ClassIndex::MESSAGEPORT, args.Holder());

  V8Proxy* proxy = V8Proxy::retrieve(imp->document()->frame());
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

CALLBACK_FUNC_DECL(MessagePortRemoveEventListener) {
  INC_STATS(L"DOM.MessagePort.RemoveEventListener()");
  MessagePort* imp = V8Proxy::ToNativeObject<MessagePort>(
      V8ClassIndex::MESSAGEPORT, args.Holder());

  V8Proxy* proxy = V8Proxy::retrieve(imp->document()->frame());
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
