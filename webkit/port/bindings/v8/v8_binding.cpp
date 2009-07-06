// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "v8_binding.h"

#include "AtomicString.h"
#include "CString.h"
#include "MathExtras.h"
#include "PlatformString.h"
#include "StringBuffer.h"

#include <v8.h>

namespace WebCore {

// WebCoreStringResource is a helper class for v8ExternalString. It is used
// to manage the life-cycle of the underlying buffer of the external string.
class WebCoreStringResource: public v8::String::ExternalStringResource {
 public:
  explicit WebCoreStringResource(const String& str)
      : impl_(str.impl()) { }

  virtual ~WebCoreStringResource() {}

  const uint16_t* data() const {
    return reinterpret_cast<const uint16_t*>(impl_.characters());
  }

  size_t length() const { return impl_.length(); }

  String webcore_string() { return impl_; }

 private:
  // A shallow copy of the string.
  // Keeps the string buffer alive until the V8 engine garbage collects it.
  String impl_;
};


String v8StringToWebCoreString(
    v8::Handle<v8::String> v8_str, bool externalize) {
  WebCoreStringResource* str_resource = static_cast<WebCoreStringResource*>(
      v8_str->GetExternalStringResource());
  if (str_resource) {
    return str_resource->webcore_string();
  }

  int length = v8_str->Length();
  if (length == 0) {
    // Avoid trying to morph empty strings, as they do not have enough room to
    // contain the external reference.
    return StringImpl::empty();
  }

  UChar* buffer;
  String result = String::createUninitialized(length, buffer);
  v8_str->Write(reinterpret_cast<uint16_t*>(buffer), 0, length);

  if (externalize) {
    WebCoreStringResource* resource = new WebCoreStringResource(result);
    if (!v8_str->MakeExternal(resource)) {
      // In case of a failure delete the external resource as it was not used.
      delete resource;
    }
  }
  return result;
}


String v8ValueToWebCoreString(v8::Handle<v8::Value> obj) {
  if (obj->IsString()) {
    v8::Handle<v8::String> v8_str = v8::Handle<v8::String>::Cast(obj);
    String webCoreString = v8StringToWebCoreString(v8_str, true);
    return webCoreString;
  } else if (obj->IsInt32()) {
    int value = obj->Int32Value();
    // Most numbers used are <= 100. Even if they aren't used
    // there's very little in using the space.
    const int kLowNumbers = 100;
    static AtomicString lowNumbers[kLowNumbers + 1];
    String webCoreString;
    if (0 <= value && value <= kLowNumbers) {
      webCoreString = lowNumbers[value];
      if (!webCoreString) {
        AtomicString valueString = AtomicString(String::number(value));
        lowNumbers[value] = valueString;
        webCoreString = valueString;
      }
    } else {
      webCoreString = String::number(value);
    }
    return webCoreString;
  } else {
    v8::TryCatch block;
    v8::Handle<v8::String> v8_str = obj->ToString();
    // Check for empty handles to handle the case where an exception
    // is thrown as part of invoking toString on the object.
    if (v8_str.IsEmpty())
      return StringImpl::empty();
    return v8StringToWebCoreString(v8_str, false);
  }
}


AtomicString v8StringToAtomicWebCoreString(v8::Handle<v8::String> v8_str) {
  String str = v8StringToWebCoreString(v8_str, true);
  return AtomicString(str);
}


AtomicString v8ValueToAtomicWebCoreString(v8::Handle<v8::Value> v8_str) {
  String str = v8ValueToWebCoreString(v8_str);
  return AtomicString(str);
}


v8::Handle<v8::String> v8String(const String& str) {
  if (!str.length())
    return v8::String::Empty();
  return v8::String::NewExternal(new WebCoreStringResource(str));
}

v8::Local<v8::String> v8ExternalString(const String& str) {
  if (!str.length())
    return v8::String::Empty();
  return v8::String::NewExternal(new WebCoreStringResource(str));
}

}  // namespace WebCore
