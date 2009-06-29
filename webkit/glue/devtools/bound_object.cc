// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include <string>

#include "V8Proxy.h"
#include "webkit/glue/devtools/bound_object.h"

using namespace WebCore;

BoundObject::BoundObject(
    v8::Handle<v8::Context> context,
    void* v8_this,
    const char* object_name)
    : context_(context),
      object_name_(object_name) {
  v8::HandleScope scope;
  v8::Context::Scope context_scope(context);
  v8_this_ = v8::Persistent<v8::External>::New(v8::External::New(v8_this));

  v8::Local<v8::FunctionTemplate> local_template =
      v8::FunctionTemplate::New(V8Proxy::checkNewLegal);
  host_template_ = v8::Persistent<v8::FunctionTemplate>::New(local_template);
  host_template_->SetClassName(v8::String::New(object_name));
}

BoundObject::~BoundObject() {
  bound_object_.Dispose();
  host_template_.Dispose();
  v8_this_.Dispose();
}

void BoundObject::AddProtoFunction(
    const char* name,
    v8::InvocationCallback callback) {
  v8::HandleScope scope;
  v8::Local<v8::Signature> signature = v8::Signature::New(host_template_);
  v8::Local<v8::ObjectTemplate> proto = host_template_->PrototypeTemplate();
  proto->Set(
      v8::String::New(name),
      v8::FunctionTemplate::New(
          callback,
          v8_this_,
          signature),
      static_cast<v8::PropertyAttribute>(v8::DontDelete));
}

void BoundObject::Build() {
  v8::HandleScope scope;
  v8::Context::Scope frame_scope(context_);

  v8::Local<v8::Function> constructor = host_template_->GetFunction();
  bound_object_ = v8::Persistent<v8::Object>::New(
      SafeAllocation::newInstance(constructor));

  v8::Handle<v8::Object> global = context_->Global();
  global->Set(v8::String::New(object_name_), bound_object_);
}
