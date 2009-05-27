// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_DEVTOOLS_BOUND_OBJECT_H_
#define WEBKIT_GLUE_DEVTOOLS_BOUND_OBJECT_H_

#include "v8.h"

// BoundObject lets you map JavaScript method calls and property accesses
// directly to C++ method calls and V8 variable access.
class BoundObject {
 public:
  BoundObject(v8::Handle<v8::Context> context,
              void* v8_this,
              const char* object_name);
  virtual ~BoundObject();

  void AddProtoFunction(const char* name, v8::InvocationCallback callback);
  void Build();

 private:
  const char* object_name_;
  v8::Handle<v8::Context> context_;
  v8::Persistent<v8::FunctionTemplate> host_template_;
  v8::Persistent<v8::External> v8_this_;
  v8::Persistent<v8::Object> bound_object_;
  DISALLOW_COPY_AND_ASSIGN(BoundObject);
};

#endif  // WEBKIT_GLUE_DEVTOOLS_BOUND_OBJECT_H_
