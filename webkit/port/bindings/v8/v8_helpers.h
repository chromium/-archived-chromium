// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HELPERS_H__
#define V8_HELPERS_H__

#include "third_party/npapi/bindings/npruntime.h"
#include <v8.h>

namespace WebCore {
  class V8Proxy;
}

// Associates an NPObject with a V8 object.
void WrapNPObject(v8::Handle<v8::Object> obj, NPObject *npobj);

// Retrieves the V8 Context from the NP context pr obj (at most 1 may be NULL).
v8::Local<v8::Context> getV8Context(NPP npp, NPObject* npobj);

// Get V8Proxy object from an NPObject.
WebCore::V8Proxy* GetV8Proxy(NPObject* npobj);

#endif  // V8_HELPERS_H__

