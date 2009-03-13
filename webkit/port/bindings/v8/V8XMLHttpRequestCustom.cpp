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
#include "v8_proxy.h"

#include "V8Document.h"
#include "V8HTMLDocument.h"
#include "V8ObjectEventListener.h"

#include "ExceptionCode.h"
#include "Frame.h"
#include "XMLHttpRequest.h"
#include "XMLHttpRequestUpload.h"

namespace WebCore {

CALLBACK_FUNC_DECL(XMLHttpRequestConstructor) {
  INC_STATS("DOM.XMLHttpRequest.Constructor");

  if (!args.IsConstructCall()) {
    V8Proxy::ThrowError(V8Proxy::TYPE_ERROR,
        "DOM object constructor cannot be called as a function.");
    return v8::Undefined();
  }
  // Expect no parameters.
  // Allocate a XMLHttpRequest object as its internal field.
  Document* doc = V8Proxy::retrieveFrame()->document();
  RefPtr<XMLHttpRequest> xhr = XMLHttpRequest::create(doc);
  V8Proxy::SetDOMWrapper(args.Holder(),
      V8ClassIndex::ToInt(V8ClassIndex::XMLHTTPREQUEST), xhr.get());
  // Add object to the wrapper map.
  xhr->ref();
  V8Proxy::SetJSWrapperForActiveDOMObject(xhr.get(),
      v8::Persistent<v8::Object>::New(args.Holder()));
  return args.Holder();
}

// XMLHttpRequest --------------------------------------------------------------

// Use an array to hold dependents. It works like a ref-counted scheme.
// A value can be added more than once to the xhr object.
static void CreateHiddenXHRDependency(v8::Local<v8::Object> xhr,
                                      v8::Local<v8::Value> value) {
  ASSERT(V8Proxy::GetDOMWrapperType(xhr) == V8ClassIndex::XMLHTTPREQUEST ||
         V8Proxy::GetDOMWrapperType(xhr) == V8ClassIndex::XMLHTTPREQUESTUPLOAD);
  v8::Local<v8::Value> cache =
      xhr->GetInternalField(V8Custom::kXMLHttpRequestCacheIndex);
  if (cache->IsNull() || cache->IsUndefined()) {
    cache = v8::Array::New();
    xhr->SetInternalField(V8Custom::kXMLHttpRequestCacheIndex, cache);
  }

  v8::Local<v8::Array> cache_array = v8::Local<v8::Array>::Cast(cache);
  cache_array->Set(v8::Integer::New(cache_array->Length()), value);
}

static void RemoveHiddenXHRDependency(v8::Local<v8::Object> xhr,
                                      v8::Local<v8::Value> value) {
  ASSERT(V8Proxy::GetDOMWrapperType(xhr) == V8ClassIndex::XMLHTTPREQUEST ||
         V8Proxy::GetDOMWrapperType(xhr) == V8ClassIndex::XMLHTTPREQUESTUPLOAD);
  v8::Local<v8::Value> cache =
      xhr->GetInternalField(V8Custom::kXMLHttpRequestCacheIndex);
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

ACCESSOR_GETTER(XMLHttpRequestOnabort) {
  INC_STATS("DOM.XMLHttpRequest.onabort._get");
  XMLHttpRequest* imp = V8Proxy::ToNativeObject<XMLHttpRequest>(
      V8ClassIndex::XMLHTTPREQUEST, info.Holder());
  if (imp->onabort()) {
    V8ObjectEventListener* listener =
        static_cast<V8ObjectEventListener*>(imp->onabort());
    v8::Local<v8::Object> v8_listener = listener->getListenerObject();
    return v8_listener;
  }
  return v8::Undefined();
}

ACCESSOR_SETTER(XMLHttpRequestOnabort) {
  INC_STATS("DOM.XMLHttpRequest.onabort._set");
  XMLHttpRequest* imp = V8Proxy::ToNativeObject<XMLHttpRequest>(
      V8ClassIndex::XMLHTTPREQUEST, info.Holder());
  if (value->IsNull()) {
    if (imp->onabort()) {
      V8ObjectEventListener* listener =
          static_cast<V8ObjectEventListener*>(imp->onabort());
      v8::Local<v8::Object> v8_listener = listener->getListenerObject();
      RemoveHiddenXHRDependency(info.Holder(), v8_listener);
    }

    // Clear the listener
    imp->setOnabort(0);
  } else {
    V8Proxy* proxy = V8Proxy::retrieve(imp->scriptExecutionContext());
    if (!proxy)
      return;

    RefPtr<EventListener> listener =
      proxy->FindOrCreateObjectEventListener(value, false);
    if (listener) {
      imp->setOnabort(listener);
      CreateHiddenXHRDependency(info.Holder(), value);
    }
  }
}

ACCESSOR_GETTER(XMLHttpRequestOnerror) {
  INC_STATS("DOM.XMLHttpRequest.onerror._get");
  XMLHttpRequest* imp = V8Proxy::ToNativeObject<XMLHttpRequest>(
      V8ClassIndex::XMLHTTPREQUEST, info.Holder());
  if (imp->onerror()) {
    RefPtr<V8ObjectEventListener> listener =
        static_cast<V8ObjectEventListener*>(imp->onerror());
    v8::Local<v8::Object> v8_listener = listener->getListenerObject();
    return v8_listener;
  }
  return v8::Undefined();
}

ACCESSOR_SETTER(XMLHttpRequestOnerror) {
  INC_STATS("DOM.XMLHttpRequest.onerror._set");
  XMLHttpRequest* imp = V8Proxy::ToNativeObject<XMLHttpRequest>(
      V8ClassIndex::XMLHTTPREQUEST, info.Holder());
  if (value->IsNull()) {
    if (imp->onerror()) {
      V8ObjectEventListener* listener =
          static_cast<V8ObjectEventListener*>(imp->onerror());
      v8::Local<v8::Object> v8_listener = listener->getListenerObject();
      RemoveHiddenXHRDependency(info.Holder(), v8_listener);
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
      imp->setOnerror(listener);
      CreateHiddenXHRDependency(info.Holder(), value);
    }
  }
}

ACCESSOR_GETTER(XMLHttpRequestOnload) {
  INC_STATS("DOM.XMLHttpRequest.onload._get");
  XMLHttpRequest* imp = V8Proxy::ToNativeObject<XMLHttpRequest>(
      V8ClassIndex::XMLHTTPREQUEST, info.Holder());
  if (imp->onload()) {
    V8ObjectEventListener* listener =
        static_cast<V8ObjectEventListener*>(imp->onload());
    v8::Local<v8::Object> v8_listener = listener->getListenerObject();
    return v8_listener;
  }
  return v8::Undefined();
}

ACCESSOR_SETTER(XMLHttpRequestOnload)
{
  INC_STATS("DOM.XMLHttpRequest.onload._set");
  XMLHttpRequest* imp = V8Proxy::ToNativeObject<XMLHttpRequest>(
      V8ClassIndex::XMLHTTPREQUEST, info.Holder());
  if (value->IsNull()) {
    if (imp->onload()) {
      V8ObjectEventListener* listener = static_cast<V8ObjectEventListener*>(imp->onload());
      v8::Local<v8::Object> v8_listener = listener->getListenerObject();
      RemoveHiddenXHRDependency(info.Holder(), v8_listener);
    }

    imp->setOnload(0);

  } else {
    V8Proxy* proxy = V8Proxy::retrieve(imp->scriptExecutionContext());
    if (!proxy)
      return;

    RefPtr<EventListener> listener =
      proxy->FindOrCreateObjectEventListener(value, false);
    if (listener) {
      imp->setOnload(listener.get());
      CreateHiddenXHRDependency(info.Holder(), value);
    }
  }
}

ACCESSOR_GETTER(XMLHttpRequestOnloadstart) {
  INC_STATS("DOM.XMLHttpRequest.onloadstart._get");
  XMLHttpRequest* imp = V8Proxy::ToNativeObject<XMLHttpRequest>(
      V8ClassIndex::XMLHTTPREQUEST, info.Holder());
  if (imp->onloadstart()) {
    V8ObjectEventListener* listener =
        static_cast<V8ObjectEventListener*>(imp->onloadstart());
    v8::Local<v8::Object> v8_listener = listener->getListenerObject();
    return v8_listener;
  }
  return v8::Undefined();
}

ACCESSOR_SETTER(XMLHttpRequestOnloadstart) {
  INC_STATS("DOM.XMLHttpRequest.onloadstart._set");
  XMLHttpRequest* imp = V8Proxy::ToNativeObject<XMLHttpRequest>(
      V8ClassIndex::XMLHTTPREQUEST, info.Holder());
  if (value->IsNull()) {
    if (imp->onloadstart()) {
      V8ObjectEventListener* listener =
          static_cast<V8ObjectEventListener*>(imp->onloadstart());
      v8::Local<v8::Object> v8_listener = listener->getListenerObject();
      RemoveHiddenXHRDependency(info.Holder(), v8_listener);
    }

    // Clear the listener
    imp->setOnloadstart(0);
  } else {
    V8Proxy* proxy = V8Proxy::retrieve(imp->scriptExecutionContext());
    if (!proxy)
      return;

    RefPtr<EventListener> listener =
      proxy->FindOrCreateObjectEventListener(value, false);
    if (listener) {
      imp->setOnloadstart(listener);
      CreateHiddenXHRDependency(info.Holder(), value);
    }
  }
}

ACCESSOR_GETTER(XMLHttpRequestOnprogress) {
  INC_STATS("DOM.XMLHttpRequest.onprogress._get");
  XMLHttpRequest* imp = V8Proxy::ToNativeObject<XMLHttpRequest>(
      V8ClassIndex::XMLHTTPREQUEST, info.Holder());
  if (imp->onprogress()) {
    V8ObjectEventListener* listener =
        static_cast<V8ObjectEventListener*>(imp->onprogress());
    v8::Local<v8::Object> v8_listener = listener->getListenerObject();
    return v8_listener;
  }
  return v8::Undefined();
}

ACCESSOR_SETTER(XMLHttpRequestOnprogress) {
  INC_STATS("DOM.XMLHttpRequest.onprogress._set");
  XMLHttpRequest* imp = V8Proxy::ToNativeObject<XMLHttpRequest>(
      V8ClassIndex::XMLHTTPREQUEST, info.Holder());
  if (value->IsNull()) {
    if (imp->onprogress()) {
      V8ObjectEventListener* listener =
          static_cast<V8ObjectEventListener*>(imp->onprogress());
      v8::Local<v8::Object> v8_listener = listener->getListenerObject();
      RemoveHiddenXHRDependency(info.Holder(), v8_listener);
    }

    // Clear the listener
    imp->setOnprogress(0);
  } else {
    V8Proxy* proxy = V8Proxy::retrieve(imp->scriptExecutionContext());
    if (!proxy)
      return;

    RefPtr<EventListener> listener =
      proxy->FindOrCreateObjectEventListener(value, false);
    if (listener) {
      imp->setOnprogress(listener);
      CreateHiddenXHRDependency(info.Holder(), value);
    }
  }
}

ACCESSOR_GETTER(XMLHttpRequestOnreadystatechange) {
  INC_STATS("DOM.XMLHttpRequest.onreadystatechange._get");
  XMLHttpRequest* imp = V8Proxy::ToNativeObject<XMLHttpRequest>(
      V8ClassIndex::XMLHTTPREQUEST, info.Holder());
  if (imp->onreadystatechange()) {
    V8ObjectEventListener* listener =
        static_cast<V8ObjectEventListener*>(imp->onreadystatechange());
    v8::Local<v8::Object> v8_listener = listener->getListenerObject();
    return v8_listener;
  }
  return v8::Undefined();
}

ACCESSOR_SETTER(XMLHttpRequestOnreadystatechange)
{
  INC_STATS("DOM.XMLHttpRequest.onreadystatechange._set");
  XMLHttpRequest* imp = V8Proxy::ToNativeObject<XMLHttpRequest>(
      V8ClassIndex::XMLHTTPREQUEST, info.Holder());
  if (value->IsNull()) {
    if (imp->onreadystatechange()) {
      V8ObjectEventListener* listener =
          static_cast<V8ObjectEventListener*>(imp->onreadystatechange());
      v8::Local<v8::Object> v8_listener = listener->getListenerObject();
      RemoveHiddenXHRDependency(info.Holder(), v8_listener);
    }

    // Clear the listener
    imp->setOnreadystatechange(0);
  } else {
    V8Proxy* proxy = V8Proxy::retrieve(imp->scriptExecutionContext());
    if (!proxy)
      return;

    RefPtr<EventListener> listener =
      proxy->FindOrCreateObjectEventListener(value, false);
    if (listener) {
      imp->setOnreadystatechange(listener.get());
      CreateHiddenXHRDependency(info.Holder(), value);
    }
  }
}

ACCESSOR_GETTER(XMLHttpRequestResponseText)
{
  // This is only needed because webkit set this getter as custom.
  // So we need a custom method to avoid forking the IDL file.
  INC_STATS("DOM.XMLHttpRequest.responsetext._get");
  XMLHttpRequest* imp = V8Proxy::ToNativeObject<XMLHttpRequest>(
      V8ClassIndex::XMLHTTPREQUEST, info.Holder());
  return v8StringOrNull(imp->responseText());
}

CALLBACK_FUNC_DECL(XMLHttpRequestAddEventListener)
{
  INC_STATS("DOM.XMLHttpRequest.addEventListener()");
  XMLHttpRequest* imp = V8Proxy::ToNativeObject<XMLHttpRequest>(
      V8ClassIndex::XMLHTTPREQUEST, args.Holder());

  V8Proxy* proxy = V8Proxy::retrieve(imp->scriptExecutionContext());
  if (!proxy)
    return v8::Undefined();

  RefPtr<EventListener> listener =
      proxy->FindOrCreateObjectEventListener(args[1], false);
  if (listener) {
    String type = ToWebCoreString(args[0]);
    bool useCapture = args[2]->BooleanValue();
    imp->addEventListener(type, listener, useCapture);

    CreateHiddenXHRDependency(args.Holder(), args[1]);
  }
  return v8::Undefined();
}

CALLBACK_FUNC_DECL(XMLHttpRequestRemoveEventListener) {
  INC_STATS("DOM.XMLHttpRequest.removeEventListener()");
  XMLHttpRequest* imp = V8Proxy::ToNativeObject<XMLHttpRequest>(
      V8ClassIndex::XMLHTTPREQUEST, args.Holder());

  V8Proxy* proxy = V8Proxy::retrieve(imp->scriptExecutionContext());
  if (!proxy)
    return v8::Undefined();  // probably leaked

  RefPtr<EventListener> listener =
    proxy->FindObjectEventListener(args[1], false);

  if (listener) {
    String type = ToWebCoreString(args[0]);
    bool useCapture = args[2]->BooleanValue();
    imp->removeEventListener(type, listener.get(), useCapture);

    RemoveHiddenXHRDependency(args.Holder(), args[1]);
  }

  return v8::Undefined();
}

CALLBACK_FUNC_DECL(XMLHttpRequestOpen)
{
  INC_STATS("DOM.XMLHttpRequest.open()");
  // Four cases:
  // open(method, url)
  // open(method, url, async)
  // open(method, url, async, user)
  // open(method, url, async, user, passwd)

  if (args.Length() < 2) {
    V8Proxy::ThrowError(V8Proxy::SYNTAX_ERROR, "Not enough arguments");
    return v8::Undefined();
  }

  // get the implementation
  XMLHttpRequest* xhr = V8Proxy::ToNativeObject<XMLHttpRequest>(
      V8ClassIndex::XMLHTTPREQUEST, args.Holder());

  // retrieve parameters
  String method = ToWebCoreString(args[0]);
  String urlstring = ToWebCoreString(args[1]);
  V8Proxy* proxy = V8Proxy::retrieve();
  KURL url = proxy->frame()->document()->completeURL(urlstring);

  bool async = (args.Length() < 3) ? true : args[2]->BooleanValue();

  ExceptionCode ec = 0;
  String user, passwd;
  if (args.Length() >= 4 && !args[3]->IsUndefined()) {
    user = valueToStringWithNullCheck(args[3]);

    if (args.Length() >= 5 && !args[4]->IsUndefined()) {
      passwd = valueToStringWithNullCheck(args[4]);
      xhr->open(method, url, async, user, passwd, ec);
    } else
      xhr->open(method, url, async, user, ec);
  } else
    xhr->open(method, url, async, ec);

  if (ec != 0) {
    V8Proxy::SetDOMException(ec);
    return v8::Handle<v8::Value>();
  }

  return v8::Undefined();
}

static bool IsDocumentType(v8::Handle<v8::Value> value)
{
    // TODO(fqian): add other document types.
    return V8Document::HasInstance(value) || V8HTMLDocument::HasInstance(value);
}

CALLBACK_FUNC_DECL(XMLHttpRequestSend)
{
    INC_STATS("DOM.XMLHttpRequest.send()");
  XMLHttpRequest* xhr = V8Proxy::ToNativeObject<XMLHttpRequest>(
        V8ClassIndex::XMLHTTPREQUEST, args.Holder());

    ExceptionCode ec = 0;
    if (args.Length() < 1)
        xhr->send(ec);
    else {
        v8::Handle<v8::Value> arg = args[0];
        // TODO(eseidel): upstream handles "File" objects too
        if (IsDocumentType(arg)) {
            v8::Handle<v8::Object> obj = v8::Handle<v8::Object>::Cast(arg);
            Document* doc = V8Proxy::DOMWrapperToNode<Document>(obj);
            ASSERT(doc);
            xhr->send(doc, ec);
        } else
            xhr->send(valueToStringWithNullCheck(arg), ec);
    }

    if (ec) {
        V8Proxy::SetDOMException(ec);
        return v8::Handle<v8::Value>();
    }

    return v8::Undefined();
}

CALLBACK_FUNC_DECL(XMLHttpRequestSetRequestHeader) {
  INC_STATS("DOM.XMLHttpRequest.setRequestHeader()");
  if (args.Length() < 2) {
    V8Proxy::ThrowError(V8Proxy::SYNTAX_ERROR, "Not enough arguments");
    return v8::Undefined();
  }

  XMLHttpRequest* imp = V8Proxy::ToNativeObject<XMLHttpRequest>(
      V8ClassIndex::XMLHTTPREQUEST, args.Holder());
  ExceptionCode ec = 0;
  String header = ToWebCoreString(args[0]);
  String value = ToWebCoreString(args[1]);
  imp->setRequestHeader(header, value, ec);
  if (ec != 0) {
    V8Proxy::SetDOMException(ec);
    return v8::Handle<v8::Value>();
  }
  return v8::Undefined();
}

CALLBACK_FUNC_DECL(XMLHttpRequestGetResponseHeader) {
  INC_STATS("DOM.XMLHttpRequest.getResponseHeader()");
  if (args.Length() < 1) {
    V8Proxy::ThrowError(V8Proxy::SYNTAX_ERROR, "Not enough arguments");
    return v8::Undefined();
  }

  XMLHttpRequest* imp = V8Proxy::ToNativeObject<XMLHttpRequest>(
      V8ClassIndex::XMLHTTPREQUEST, args.Holder());
  ExceptionCode ec = 0;
  String header = ToWebCoreString(args[0]);
  String result = imp->getResponseHeader(header, ec);
  if (ec != 0) {
    V8Proxy::SetDOMException(ec);
    return v8::Handle<v8::Value>();
  }
  return v8StringOrNull(result);
}

CALLBACK_FUNC_DECL(XMLHttpRequestOverrideMimeType)
{
  INC_STATS("DOM.XMLHttpRequest.overrideMimeType()");
  if (args.Length() < 1) {
    V8Proxy::ThrowError(V8Proxy::SYNTAX_ERROR, "Not enough arguments");
    return v8::Undefined();
  }

  XMLHttpRequest* imp = V8Proxy::ToNativeObject<XMLHttpRequest>(
      V8ClassIndex::XMLHTTPREQUEST, args.Holder());
  String value = ToWebCoreString(args[0]);
  imp->overrideMimeType(value);
  return v8::Undefined();
}

CALLBACK_FUNC_DECL(XMLHttpRequestDispatchEvent)
{
  INC_STATS("DOM.XMLHttpRequest.dispatchEvent()");
  return v8::Undefined();
}


// XMLHttpRequestUpload --------------------------------------------------------

ACCESSOR_GETTER(XMLHttpRequestUploadOnabort) {
  INC_STATS("DOM.XMLHttpRequestUpload.onabort._get");
  XMLHttpRequestUpload* imp = V8Proxy::ToNativeObject<XMLHttpRequestUpload>(
      V8ClassIndex::XMLHTTPREQUESTUPLOAD, info.Holder());
  if (imp->onabort()) {
    V8ObjectEventListener* listener =
        static_cast<V8ObjectEventListener*>(imp->onabort());
    v8::Local<v8::Object> v8_listener = listener->getListenerObject();
    return v8_listener;
  }
  return v8::Undefined();
}

ACCESSOR_SETTER(XMLHttpRequestUploadOnabort) {
  INC_STATS("DOM.XMLHttpRequestUpload.onabort._set");
  XMLHttpRequestUpload* imp = V8Proxy::ToNativeObject<XMLHttpRequestUpload>(
      V8ClassIndex::XMLHTTPREQUESTUPLOAD, info.Holder());
  if (value->IsNull()) {
    if (imp->onabort()) {
      V8ObjectEventListener* listener =
          static_cast<V8ObjectEventListener*>(imp->onabort());
      v8::Local<v8::Object> v8_listener = listener->getListenerObject();
      RemoveHiddenXHRDependency(info.Holder(), v8_listener);
    }

    // Clear the listener
    imp->setOnabort(0);
  } else {
    XMLHttpRequest* xmlhttprequest = imp->associatedXMLHttpRequest();
    V8Proxy* proxy =
        V8Proxy::retrieve(xmlhttprequest->scriptExecutionContext());
    if (!proxy)
      return;

    RefPtr<EventListener> listener =
      proxy->FindOrCreateObjectEventListener(value, false);
    if (listener) {
      imp->setOnabort(listener);
      CreateHiddenXHRDependency(info.Holder(), value);
    }
  }
}

ACCESSOR_GETTER(XMLHttpRequestUploadOnerror) {
  INC_STATS("DOM.XMLHttpRequestUpload.onerror._get");
  XMLHttpRequestUpload* imp = V8Proxy::ToNativeObject<XMLHttpRequestUpload>(
      V8ClassIndex::XMLHTTPREQUESTUPLOAD, info.Holder());
  if (imp->onerror()) {
    V8ObjectEventListener* listener =
        static_cast<V8ObjectEventListener*>(imp->onerror());
    v8::Local<v8::Object> v8_listener = listener->getListenerObject();
    return v8_listener;
  }
  return v8::Undefined();
}

ACCESSOR_SETTER(XMLHttpRequestUploadOnerror) {
  INC_STATS("DOM.XMLHttpRequestUpload.onerror._set");
  XMLHttpRequestUpload* imp = V8Proxy::ToNativeObject<XMLHttpRequestUpload>(
      V8ClassIndex::XMLHTTPREQUESTUPLOAD, info.Holder());
  if (value->IsNull()) {
    if (imp->onerror()) {
      V8ObjectEventListener* listener =
          static_cast<V8ObjectEventListener*>(imp->onerror());
      v8::Local<v8::Object> v8_listener = listener->getListenerObject();
      RemoveHiddenXHRDependency(info.Holder(), v8_listener);
    }

    // Clear the listener
    imp->setOnerror(0);
  } else {
    XMLHttpRequest* xmlhttprequest = imp->associatedXMLHttpRequest();
    V8Proxy* proxy = V8Proxy::retrieve(xmlhttprequest->scriptExecutionContext());
    if (!proxy)
      return;

    RefPtr<EventListener> listener =
      proxy->FindOrCreateObjectEventListener(value, false);
    if (listener) {
      imp->setOnerror(listener);
      CreateHiddenXHRDependency(info.Holder(), value);
    }
  }
}

ACCESSOR_GETTER(XMLHttpRequestUploadOnload) {
  INC_STATS("DOM.XMLHttpRequestUpload.onload._get");
  XMLHttpRequestUpload* imp = V8Proxy::ToNativeObject<XMLHttpRequestUpload>(
      V8ClassIndex::XMLHTTPREQUESTUPLOAD, info.Holder());
  if (imp->onload()) {
    V8ObjectEventListener* listener =
        static_cast<V8ObjectEventListener*>(imp->onload());
    v8::Local<v8::Object> v8_listener = listener->getListenerObject();
    return v8_listener;
  }
  return v8::Undefined();
}

ACCESSOR_SETTER(XMLHttpRequestUploadOnload) {
  INC_STATS("DOM.XMLHttpRequestUpload.onload._set");
  XMLHttpRequestUpload* imp = V8Proxy::ToNativeObject<XMLHttpRequestUpload>(
      V8ClassIndex::XMLHTTPREQUESTUPLOAD, info.Holder());
  if (value->IsNull()) {
    if (imp->onload()) {
      V8ObjectEventListener* listener =
          static_cast<V8ObjectEventListener*>(imp->onload());
      v8::Local<v8::Object> v8_listener = listener->getListenerObject();
      RemoveHiddenXHRDependency(info.Holder(), v8_listener);
    }

    // Clear the listener
    imp->setOnload(0);
  } else {
    XMLHttpRequest* xmlhttprequest = imp->associatedXMLHttpRequest();
    V8Proxy* proxy = V8Proxy::retrieve(xmlhttprequest->scriptExecutionContext());
    if (!proxy)
      return;

    RefPtr<EventListener> listener =
      proxy->FindOrCreateObjectEventListener(value, false);
    if (listener) {
      imp->setOnload(listener);
      CreateHiddenXHRDependency(info.Holder(), value);
    }
  }
}

ACCESSOR_GETTER(XMLHttpRequestUploadOnloadstart) {
  INC_STATS("DOM.XMLHttpRequestUpload.onloadstart._get");
  XMLHttpRequestUpload* imp = V8Proxy::ToNativeObject<XMLHttpRequestUpload>(
      V8ClassIndex::XMLHTTPREQUESTUPLOAD, info.Holder());
  if (imp->onloadstart()) {
    V8ObjectEventListener* listener =
        static_cast<V8ObjectEventListener*>(imp->onloadstart());
    v8::Local<v8::Object> v8_listener = listener->getListenerObject();
    return v8_listener;
  }
  return v8::Undefined();
}

ACCESSOR_SETTER(XMLHttpRequestUploadOnloadstart) {
  INC_STATS("DOM.XMLHttpRequestUpload.onloadstart._set");
  XMLHttpRequestUpload* imp = V8Proxy::ToNativeObject<XMLHttpRequestUpload>(
      V8ClassIndex::XMLHTTPREQUESTUPLOAD, info.Holder());
  if (value->IsNull()) {
    if (imp->onloadstart()) {
      V8ObjectEventListener* listener =
          static_cast<V8ObjectEventListener*>(imp->onloadstart());
      v8::Local<v8::Object> v8_listener = listener->getListenerObject();
      RemoveHiddenXHRDependency(info.Holder(), v8_listener);
    }

    // Clear the listener
    imp->setOnloadstart(0);
  } else {
    XMLHttpRequest* xmlhttprequest = imp->associatedXMLHttpRequest();
    V8Proxy* proxy = V8Proxy::retrieve(xmlhttprequest->scriptExecutionContext());
    if (!proxy)
      return;

    RefPtr<EventListener> listener =
      proxy->FindOrCreateObjectEventListener(value, false);
    if (listener) {
      imp->setOnloadstart(listener);
      CreateHiddenXHRDependency(info.Holder(), value);
    }
  }
}

ACCESSOR_GETTER(XMLHttpRequestUploadOnprogress) {
  INC_STATS("DOM.XMLHttpRequestUpload.onprogress._get");
  XMLHttpRequestUpload* imp = V8Proxy::ToNativeObject<XMLHttpRequestUpload>(
      V8ClassIndex::XMLHTTPREQUESTUPLOAD, info.Holder());
  if (imp->onprogress()) {
    V8ObjectEventListener* listener =
        static_cast<V8ObjectEventListener*>(imp->onprogress());
    v8::Local<v8::Object> v8_listener = listener->getListenerObject();
    return v8_listener;
  }
  return v8::Undefined();
}

ACCESSOR_SETTER(XMLHttpRequestUploadOnprogress) {
  INC_STATS("DOM.XMLHttpRequestUpload.onprogress._set");
  XMLHttpRequestUpload* imp = V8Proxy::ToNativeObject<XMLHttpRequestUpload>(
      V8ClassIndex::XMLHTTPREQUESTUPLOAD, info.Holder());
  if (value->IsNull()) {
    if (imp->onprogress()) {
      V8ObjectEventListener* listener =
          static_cast<V8ObjectEventListener*>(imp->onprogress());
      v8::Local<v8::Object> v8_listener = listener->getListenerObject();
      RemoveHiddenXHRDependency(info.Holder(), v8_listener);
    }

    // Clear the listener
    imp->setOnprogress(0);
  } else {
    XMLHttpRequest* xmlhttprequest = imp->associatedXMLHttpRequest();
    V8Proxy* proxy = V8Proxy::retrieve(xmlhttprequest->scriptExecutionContext());
    if (!proxy)
      return;

    RefPtr<EventListener> listener =
      proxy->FindOrCreateObjectEventListener(value, false);
    if (listener) {
      imp->setOnprogress(listener);
      CreateHiddenXHRDependency(info.Holder(), value);
    }
  }
}

CALLBACK_FUNC_DECL(XMLHttpRequestUploadAddEventListener) {
  INC_STATS("DOM.XMLHttpRequestUpload.addEventListener()");
  XMLHttpRequestUpload* imp = V8Proxy::ToNativeObject<XMLHttpRequestUpload>(
      V8ClassIndex::XMLHTTPREQUESTUPLOAD, args.Holder());

  XMLHttpRequest* xmlhttprequest = imp->associatedXMLHttpRequest();
  V8Proxy* proxy = V8Proxy::retrieve(xmlhttprequest->scriptExecutionContext());
  if (!proxy)
    return v8::Undefined();

  RefPtr<EventListener> listener =
      proxy->FindOrCreateObjectEventListener(args[1], false);
  if (listener) {
    String type = ToWebCoreString(args[0]);
    bool useCapture = args[2]->BooleanValue();
    imp->addEventListener(type, listener, useCapture);

    CreateHiddenXHRDependency(args.Holder(), args[1]);
  }
  return v8::Undefined();
}

CALLBACK_FUNC_DECL(XMLHttpRequestUploadRemoveEventListener) {
  INC_STATS("DOM.XMLHttpRequestUpload.removeEventListener()");
  XMLHttpRequestUpload* imp = V8Proxy::ToNativeObject<XMLHttpRequestUpload>(
      V8ClassIndex::XMLHTTPREQUESTUPLOAD, args.Holder());

  XMLHttpRequest* xmlhttprequest = imp->associatedXMLHttpRequest();
  V8Proxy* proxy = V8Proxy::retrieve(xmlhttprequest->scriptExecutionContext());
  if (!proxy)
    return v8::Undefined();  // probably leaked

  RefPtr<EventListener> listener =
    proxy->FindObjectEventListener(args[1], false);

  if (listener) {
    String type = ToWebCoreString(args[0]);
    bool useCapture = args[2]->BooleanValue();
    imp->removeEventListener(type, listener.get(), useCapture);

    RemoveHiddenXHRDependency(args.Holder(), args[1]);
  }

  return v8::Undefined();
}

CALLBACK_FUNC_DECL(XMLHttpRequestUploadDispatchEvent) {
  INC_STATS("DOM.XMLHttpRequestUpload.dispatchEvent()");
  V8Proxy::SetDOMException(NOT_SUPPORTED_ERR);
  return v8::Undefined();
}

}  // namespace WebCore
