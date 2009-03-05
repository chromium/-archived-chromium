// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_BINDING_H__
#define V8_BINDING_H__

#include "config.h"

#include "AtomicString.h"
#include "CString.h"
#include "MathExtras.h"
#include "PlatformString.h"
#include "StringBuffer.h"

#include <v8.h>

namespace WebCore {

// The string returned by this function is still owned by the argument
// and will be deallocated when the argument is deallocated.
inline const uint16_t* FromWebCoreString(const String& str) {
  return reinterpret_cast<const uint16_t*>(str.characters());
}

// WebCoreStringResource is a helper class for v8ExternalString. It is used
// to manage the life-cycle of the underlying buffer of the external string.
class WebCoreStringResource: public v8::String::ExternalStringResource {
 public:
  explicit WebCoreStringResource(const String& str)
  : external_string_(str) {
  }

  ~WebCoreStringResource() {}

  const uint16_t* data() const { return FromWebCoreString(external_string_); }

  size_t length() const { return external_string_.length(); }

  String webcore_string() { return external_string_; }

 private:
  // A shallow copy of the string.
  // Keeps the string buffer alive until the V8 engine garbage collects it.
  String external_string_;
};

// Convert a v8::String to a WebCore::String. If the V8 string is not already
// an external string then it is transformed into an external string at this
// point to avoid repeated conversions.
inline String ToWebCoreString(v8::Handle<v8::Value> obj) {
  bool morph = false;
  bool atomic = false;
  v8::TryCatch block;
  v8::Local<v8::String> v8_str = obj->ToString();
  if (v8_str.IsEmpty())
    return "";

  if (v8_str->IsExternal()) {
    WebCoreStringResource* str_resource = static_cast<WebCoreStringResource*>(
        v8_str->GetExternalStringResource());
    return str_resource->webcore_string();
  }

  int length = v8_str->Length();
  if (length == 0) {
    // Avoid trying to morph empty strings, as they do not have enough room to
    // contain the external reference.
    return "";
  }

  // Copy the characters from the v8::String into a WebCore::String and allocate
  // an external resource which will be attached to the v8::String.
  StringBuffer buf(length);
  v8_str->Write(reinterpret_cast<uint16_t*>(buf.characters()), 0, length);
  String result;
  if (atomic) {
    result = AtomicString(buf.characters(), length);
  } else {
    result = StringImpl::adopt(buf);
  }
  if (morph) {
    WebCoreStringResource* resource = new WebCoreStringResource(result);
    if (!v8_str->MakeExternal(resource)) {
      // In case of a failure delete the external resource as it was not used.
      delete resource;
    }
  }
  return result;
}

inline String valueToStringWithNullCheck(v8::Handle<v8::Value> value) {
  if (value->IsNull()) return String();
  return ToWebCoreString(value);
}

inline String valueToStringWithNullOrUndefinedCheck(
    v8::Handle<v8::Value> value) {
  if (value->IsNull() || value->IsUndefined()) return String();
  return ToWebCoreString(value);
}

// Convert a value to a 32-bit integer.  The conversion fails if the
// value cannot be converted to an integer or converts to nan or to an
// infinity.
// FIXME: Rename to toInt32() once V8 bindings migration is complete.
inline int ToInt32(v8::Handle<v8::Value> value, bool& ok) {
  ok = true;

  // Fast case.  The value is already a 32-bit integer.
  if (value->IsInt32()) {
    return value->Int32Value();
  }

  // Can the value be converted to a number?
  v8::Local<v8::Number> number_object = value->ToNumber();
  if (number_object.IsEmpty()) {
    ok = false;
    return 0;
  }

  // Does the value convert to nan or to an infinity?
  double number_value = number_object->Value();
  if (isnan(number_value) || isinf(number_value)) {
    ok = false;
    return 0;
  }

  // Can the value be converted to a 32-bit integer?
  v8::Local<v8::Int32> int_value = value->ToInt32();
  if (int_value.IsEmpty()) {
    ok = false;
    return 0;
  }

  // Return the result of the int32 conversion.
  return int_value->Value();
}

// Convert a value to a 32-bit integer assuming the conversion cannot fail.
// FIXME: Rename to toInt32() once V8 bindings migration is complete.
inline int ToInt32(v8::Handle<v8::Value> value) {
  bool ok;
  return ToInt32(value, ok);
}

inline String ToString(const String& string) {
  return string;
}

// If a WebCore string length is greater than the threshold,
// v8String creates an external string to avoid allocating
// the string in the large object space (which has a high memory overhead).
static const unsigned int kV8ExternalStringThreshold = 2048;

// Convert a string to a V8 string.
inline v8::Handle<v8::String> v8String(const String& str) {
  if (str.length() <= kV8ExternalStringThreshold) {
    return v8::String::New(FromWebCoreString(str), str.length());
  } else {
    return v8::String::NewExternal(new WebCoreStringResource(str));
  }
}

inline v8::Handle<v8::String> v8UndetectableString(const String& str) {
  return v8::String::NewUndetectable(FromWebCoreString(str), str.length());
}

// Return a V8 external string that shares the underlying buffer with the given
// WebCore string. The reference counting mechanism is used to keep the
// underlying buffer alive while the string is still live in the V8 engine.
inline v8::Local<v8::String> v8ExternalString(const String& str) {
  return v8::String::NewExternal(new WebCoreStringResource(str));
}

inline v8::Handle<v8::Value> v8StringOrNull(const String& str) {
  return str.isNull()
    ? v8::Handle<v8::Value>(v8::Null())
    : v8::Handle<v8::Value>(v8String(str));
}

inline v8::Handle<v8::Value> v8StringOrUndefined(const String& str) {
  return str.isNull()
    ? v8::Handle<v8::Value>(v8::Undefined())
    : v8::Handle<v8::Value>(v8String(str));
}

inline v8::Handle<v8::Value> v8StringOrFalse(const String& str) {
  return str.isNull()
    ? v8::Handle<v8::Value>(v8::False())
    : v8::Handle<v8::Value>(v8String(str));
}

}  // namespace WebCore

#endif  // V8_BINDING_H__

