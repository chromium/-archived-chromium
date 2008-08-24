// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_NPOBJECT_H__
#define V8_NPOBJECT_H__

#include <v8.h>
#include "third_party/npapi/bindings/npruntime.h"

// These functions can be replaced by normal JS operation.
// Getters
v8::Handle<v8::Value> NPObjectNamedPropertyGetter(v8::Local<v8::String> name,
                                                  const v8::AccessorInfo& info);
v8::Handle<v8::Value> NPObjectIndexedPropertyGetter(uint32_t index,
                                                    const v8::AccessorInfo& info);
v8::Handle<v8::Value> NPObjectGetNamedProperty(v8::Local<v8::Object> self,
                                               v8::Local<v8::String> name);
v8::Handle<v8::Value> NPObjectGetIndexedProperty(v8::Local<v8::Object> self,
                                                 uint32_t index);

// Setters
v8::Handle<v8::Value> NPObjectNamedPropertySetter(v8::Local<v8::String> name,
                                                  v8::Local<v8::Value> value,
                                                  const v8::AccessorInfo& info);
v8::Handle<v8::Value> NPObjectIndexedPropertySetter(uint32_t index,
                                                    const v8::AccessorInfo& info);
v8::Handle<v8::Value> NPObjectSetNamedProperty(v8::Local<v8::Object> self,
                                               v8::Local<v8::String> name,
                                               v8::Local<v8::Value> value);
v8::Handle<v8::Value> NPObjectSetIndexedProperty(v8::Local<v8::Object> self,
                                                 uint32_t index,
                                                 v8::Local<v8::Value> value);

v8::Handle<v8::Value> NPObjectInvokeDefaultHandler(const v8::Arguments& args);

// Get a wrapper for a NPObject.
// If the object is already wrapped, the pre-existing wrapper
// will be returned.
// If the object is not wrapped, wrap it, and give V8 a weak
// reference to the wrapper which will cleanup when there are
// no more JS references to the object.
v8::Local<v8::Object> CreateV8ObjectForNPObject(NPObject* object,
                                                NPObject *root);

// Tell V8 to forcibly remove an object.
// This is used at plugin teardown so that the caller can
// aggressively unload the plugin library.  After calling this
// function, the persistent handle to the wrapper will be
// gone, and the wrapped NPObject will be removed so that
// it cannot be referred to.
void ForgetV8ObjectForNPObject(NPObject*object);

#endif  // V8_NPOBJECT_H__

