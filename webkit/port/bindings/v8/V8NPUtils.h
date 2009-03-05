// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef v8_np_utils_h
#define v8_np_utils_h

#include <v8.h>
#include "third_party/npapi/bindings/npruntime.h"

namespace WebCore {
    class Frame;
}

// Convert a V8 Value of any type (string, bool, object, etc) to a NPVariant.
void convertV8ObjectToNPVariant(v8::Local<v8::Value> object, NPObject *owner, NPVariant* result);

// Convert a NPVariant (string, bool, object, etc) back to a V8 Value.
// The owner object is the NPObject which relates to the object, if the object
// is an Object.  The created NPObject will be tied to the lifetime of the
// owner.
v8::Handle<v8::Value> convertNPVariantToV8Object(const NPVariant* value, NPObject* owner);

// Helper function to create an NPN String Identifier from a v8 string.
NPIdentifier getStringIdentifier(v8::Handle<v8::String> str);

#endif // v8_np_utils_h

