// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains definitions for CppBoundClass

// Here's the control flow of a JS method getting forwarded to a class.
// - Something calls our NPObject with a function like "Invoke".
// - CppNPObject's static invoke() function forwards it to its attached
//   CppBoundClass's Invoke() method.
// - CppBoundClass has then overridden Invoke() to look up the function
//   name in its internal map of methods, and then calls the appropriate
//   method.

#include "config.h"

#include "base/compiler_specific.h"
#include "base/logging.h"

#include "webkit/glue/cpp_bound_class.h"
#include "webkit/glue/webframe.h"

// This is required for the KJS build due to an artifact of the
// npruntime_priv.h file from JavaScriptCore/bindings.
MSVC_PUSH_DISABLE_WARNING(4067)
#include "npruntime_priv.h"
MSVC_POP_WARNING()

#if USE(JSC)
#pragma warning(push, 0)
#include <runtime/JSLock.h>
#pragma warning(pop)
#endif

// Our special NPObject type.  We extend an NPObject with a pointer to a
// CppBoundClass, which is just a C++ interface that we forward all NPObject
// callbacks to.
struct CppNPObject {
  NPObject parent;  // This must be the first field in the struct.
  CppBoundClass* bound_class;

  //
  // All following objects and functions are static, and just used to interface
  // with NPObject/NPClass.
  //

  // An NPClass associates static functions of CppNPObject with the
  // function pointers used by the JS runtime.
  static NPClass np_class_;

  // Allocate a new NPObject with the specified class.
  static NPObject* allocate(NPP npp, NPClass* aClass);

  // Free an object.
  static void deallocate(NPObject* obj);

  // Returns true if the C++ class associated with this NPObject exposes the
  // given property.  Called by the JS runtime.
  static bool hasProperty(NPObject *obj, NPIdentifier ident);

  // Returns true if the C++ class associated with this NPObject exposes the
  // given method.  Called by the JS runtime.
  static bool hasMethod(NPObject *obj, NPIdentifier ident);

  // If the given method is exposed by the C++ class associated with this
  // NPObject, invokes it with the given args and returns a result.  Otherwise,
  // returns "undefined" (in the JavaScript sense).  Called by the JS runtime.
  static bool invoke(NPObject *obj, NPIdentifier ident,
                     const NPVariant *args, uint32_t arg_count,
                     NPVariant *result);

  // If the given property is exposed by the C++ class associated with this
  // NPObject, returns its value.  Otherwise, returns "undefined" (in the
  // JavaScript sense).  Called by the JS runtime.
  static bool getProperty(NPObject *obj, NPIdentifier ident,
                          NPVariant *result);

  // If the given property is exposed by the C++ class associated with this
  // NPObject, sets its value.  Otherwise, does nothing. Called by the JS
  // runtime.
  static bool setProperty(NPObject *obj, NPIdentifier ident,
                          const NPVariant *value);
};

// Build CppNPObject's static function pointers into an NPClass, for use
// in constructing NPObjects for the C++ classes.
NPClass CppNPObject::np_class_ = {
  NP_CLASS_STRUCT_VERSION,
  CppNPObject::allocate,
  CppNPObject::deallocate,
  /* NPInvalidateFunctionPtr */ NULL,
  CppNPObject::hasMethod,
  CppNPObject::invoke,
  /* NPInvokeDefaultFunctionPtr */ NULL,
  CppNPObject::hasProperty,
  CppNPObject::getProperty,
  CppNPObject::setProperty,
  /* NPRemovePropertyFunctionPtr */ NULL
};

/* static */ NPObject* CppNPObject::allocate(NPP npp, NPClass* aClass) {
  CppNPObject* obj = new CppNPObject;
  // obj->parent will be initialized by the NPObject code calling this.
  obj->bound_class = NULL;
  return &obj->parent;
}

/* static */ void CppNPObject::deallocate(NPObject* np_obj) {
  CppNPObject* obj = reinterpret_cast<CppNPObject*>(np_obj);
  delete obj;
}

/* static */ bool CppNPObject::hasMethod(NPObject* np_obj,
                                         NPIdentifier ident) {
  CppNPObject* obj = reinterpret_cast<CppNPObject*>(np_obj);
  return obj->bound_class->HasMethod(ident);
}

/* static */ bool CppNPObject::hasProperty(NPObject* np_obj,
                                           NPIdentifier ident) {
  CppNPObject* obj = reinterpret_cast<CppNPObject*>(np_obj);
  return obj->bound_class->HasProperty(ident);
}

/* static */ bool CppNPObject::invoke(NPObject* np_obj, NPIdentifier ident,
                                      const NPVariant* args, uint32_t arg_count,
                                      NPVariant* result) {
  CppNPObject* obj = reinterpret_cast<CppNPObject*>(np_obj);
  return obj->bound_class->Invoke(ident, args, arg_count, result);
}

/* static */ bool CppNPObject::getProperty(NPObject* np_obj,
                                           NPIdentifier ident,
                                           NPVariant* result) {
  CppNPObject* obj = reinterpret_cast<CppNPObject*>(np_obj);
  return obj->bound_class->GetProperty(ident, result);
}

/* static */ bool CppNPObject::setProperty(NPObject* np_obj,
                                           NPIdentifier ident,
                                           const NPVariant* value) {
  CppNPObject* obj = reinterpret_cast<CppNPObject*>(np_obj);
  return obj->bound_class->SetProperty(ident, value);
}

CppBoundClass::~CppBoundClass() {
  for (MethodList::iterator i = methods_.begin(); i != methods_.end(); ++i)
    delete i->second;

  // Unregister ourselves if we were bound to a frame.
#if USE(V8)
  if (bound_to_frame_)
    _NPN_UnregisterObject(NPVARIANT_TO_OBJECT(self_variant_));
#endif
}

bool CppBoundClass::HasMethod(NPIdentifier ident) const {
  return (methods_.find(ident) != methods_.end());
}

bool CppBoundClass::HasProperty(NPIdentifier ident) const {
  return (properties_.find(ident) != properties_.end());
}

bool CppBoundClass::Invoke(NPIdentifier ident,
                              const NPVariant* args,
                              size_t arg_count,
                              NPVariant* result) {
  MethodList::const_iterator method = methods_.find(ident);
  Callback* callback;
  if (method == methods_.end()) {
    if (fallback_callback_.get()) {
      callback = fallback_callback_.get();
    } else {
      VOID_TO_NPVARIANT(*result);
      return false;
    }
  } else {
    callback = (*method).second;
  }

  // Build a CppArgumentList argument vector from the NPVariants coming in.
  CppArgumentList cpp_args(arg_count);
  for (size_t i = 0; i < arg_count; i++)
    cpp_args[i].Set(args[i]);

  CppVariant cpp_result;
  callback->Run(cpp_args, &cpp_result);

  cpp_result.CopyToNPVariant(result);
  return true;
}

bool CppBoundClass::GetProperty(NPIdentifier ident, NPVariant* result) const {
  PropertyList::const_iterator prop = properties_.find(ident);
  if (prop == properties_.end()) {
    VOID_TO_NPVARIANT(*result);
    return false;
  }

  const CppVariant* cpp_value = (*prop).second;
  cpp_value->CopyToNPVariant(result);
  return true;
}

bool CppBoundClass::SetProperty(NPIdentifier ident,
                                   const NPVariant* value) {
  PropertyList::iterator prop = properties_.find(ident);
  if (prop == properties_.end())
    return false;

  (*prop).second->Set(*value);
  return true;
}

void CppBoundClass::BindCallback(std::string name, Callback* callback) {
  // NPUTF8 is a typedef for char, so this cast is safe.
  NPIdentifier ident = NPN_GetStringIdentifier((const NPUTF8*)name.c_str());
  MethodList::iterator old_callback = methods_.find(ident);
  if (old_callback != methods_.end())
    delete old_callback->second;
  methods_[ident] = callback;
}

void CppBoundClass::BindProperty(std::string name, CppVariant* prop) {
  // NPUTF8 is a typedef for char, so this cast is safe.
  NPIdentifier ident = NPN_GetStringIdentifier((const NPUTF8*)name.c_str());
  properties_[ident] = prop;
}

bool CppBoundClass::IsMethodRegistered(std::string name) const {
  // NPUTF8 is a typedef for char, so this cast is safe.
  NPIdentifier ident = NPN_GetStringIdentifier((const NPUTF8*)name.c_str());
  MethodList::const_iterator callback = methods_.find(ident);
  return (callback != methods_.end());
}

CppVariant* CppBoundClass::GetAsCppVariant() {
  if (!self_variant_.isObject()) {
    // Create an NPObject using our static NPClass.  The first argument (a
    // plugin's instance handle) is passed through to the allocate function
    // directly, and we don't use it, so it's ok to be 0.
    NPObject* np_obj = NPN_CreateObject(0, &CppNPObject::np_class_);
    CppNPObject* obj = reinterpret_cast<CppNPObject*>(np_obj);
    obj->bound_class = this;
    self_variant_.Set(np_obj);
    NPN_ReleaseObject(np_obj);  // CppVariant takes the reference.
  }
  DCHECK(self_variant_.isObject());
  return &self_variant_;
}

void CppBoundClass::BindToJavascript(WebFrame* frame,
                                     const std::wstring& classname) {
#if USE(JSC)
  JSC::JSLock lock(false);
#endif

  // BindToWindowObject will take its own reference to the NPObject, and clean
  // up after itself.  It will also (indirectly) register the object with V8,
  // so we must remember this so we can unregister it when we're destroyed.
  frame->BindToWindowObject(classname, NPVARIANT_TO_OBJECT(*GetAsCppVariant()));
  bound_to_frame_ = true;
}
