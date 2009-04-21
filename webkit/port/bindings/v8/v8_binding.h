// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_BINDING_H__
#define V8_BINDING_H__

#include "config.h"

#include "MathExtras.h"
#include "PlatformString.h"

#include <v8.h>

namespace WebCore {

// The string returned by this function is still owned by the argument
// and will be deallocated when the argument is deallocated.
inline const uint16_t* FromWebCoreString(const String& str) {
  return reinterpret_cast<const uint16_t*>(str.characters());
}

// Convert v8 types to a WebCore::String. If the V8 string is not already
// an external string then it is transformed into an external string at this
// point to avoid repeated conversions.
String v8StringToWebCoreString(
    v8::Handle<v8::String> obj, bool externalize);
String v8ValueToWebCoreString(v8::Handle<v8::Value> obj);

// TODO(mbelshe): drop this in favor of the type specific
//    v8ValueToWebCoreString when we rework the code generation.
inline String ToWebCoreString(v8::Handle<v8::Value> obj) {
  return v8ValueToWebCoreString(obj);
}

inline String ToWebCoreString(v8::Handle<v8::String> string) {
  return v8StringToWebCoreString(string, true);
}

// Convert v8 types to a WebCore::AtomicString.
AtomicString v8StringToAtomicWebCoreString(v8::Handle<v8::String> obj);
AtomicString v8ValueToAtomicWebCoreString(v8::Handle<v8::Value> obj);

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

// Convert a string to a V8 string.
v8::Handle<v8::String> v8String(const String& str);

inline v8::Handle<v8::String> v8UndetectableString(const String& str) {
  return v8::String::NewUndetectable(FromWebCoreString(str), str.length());
}

// Return a V8 external string that shares the underlying buffer with the given
// WebCore string. The reference counting mechanism is used to keep the
// underlying buffer alive while the string is still live in the V8 engine.
v8::Local<v8::String> v8ExternalString(const String& str);

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
