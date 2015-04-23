/*
 * Copyright 2009, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


// Code relating to interoperation of V8 JavaScript engine with NPAPI.

#ifndef O3D_PLUGIN_CROSS_NP_V8_BRIDGE_H_
#define O3D_PLUGIN_CROSS_NP_V8_BRIDGE_H_

#include <npapi.h>
#include <npruntime.h>
#include <map>

#include "base/cross/std_hash.h"
#include "core/cross/error_status.h"
#include "core/cross/service_dependency.h"
#include "core/cross/types.h"

// The XLib header files define these preprocessor macros which v8 uses as
// identifiers. Need to undefine them before including v8.
#ifdef None
#undef None
#endif

#ifdef True
#undef True
#endif

#ifdef False
#undef False
#endif

#ifdef Value
#undef Value
#endif

#include "v8/include/v8.h"

namespace o3d {

// Smart pointer for NPObjects that automatically retains and releases the
// reference count.
template <typename T>
class NPObjectPtr {
 public:
  NPObjectPtr()
      : owned(true),
        object_(NULL) {
  }

  template <typename U>
  explicit NPObjectPtr(U* object)
      : object_(object) {
    Retain();
  }

  NPObjectPtr(const NPObjectPtr& rhs)
      : object_(rhs.object_) {
    Retain();
  }

  template <typename U>
  explicit NPObjectPtr(const NPObjectPtr<U>& rhs)
      : object_(rhs.Get()) {
    Retain();
  }

  ~NPObjectPtr() {
    Release();
  }

  template <typename U>
  NPObjectPtr& operator=(U* rhs) {
    Release();
    object_ = rhs;
    Retain();
    return *this;
  }

  NPObjectPtr& operator=(const NPObjectPtr& rhs) {
    Release();
    object_ = rhs.object_;
    Retain();
    return *this;
  }

  template <typename U>
  NPObjectPtr& operator=(const NPObjectPtr<U>& rhs) {
    Release();
    object_ = rhs.Get();
    Retain();
    return *this;
  }

  bool operator<(const NPObjectPtr& rhs) const {
    return object_ < rhs.object_;
  }

  bool operator==(const NPObjectPtr& rhs) const {
    return object_ == rhs.object_;
  }

  T* operator->() const {
    return object_;
  }

  T* Get() const {
    return object_;
  }

  bool IsNull() const {
    return object_ == NULL;
  }

  void Clear() {
    Release();
    object_ = NULL;
  }

  // Does not increment the reference count. When a function returns a pointer
  // to an NPObject, the rule is that its reference count has already been
  // incremented on behalf of the caller.
  static NPObjectPtr AttachToReturned(T* object) {
    NPObjectPtr result(object);
    result.Release();
    return result;
  }

  // Calling this prevents the NPObject's reference count from being decremented
  // by this smart pointer when it is destroyed or a new reference is assigned.
  T* Disown() const {
    owned = false;
    return object_;
  }

 private:
  void Retain() {
    owned = true;
    if (object_ != NULL) {
      NPN_RetainObject(object_);
    }
  }

  void Release() {
    if (owned && object_ != NULL) {
      NPN_ReleaseObject(object_);
    }
  }

  mutable bool owned;
  T* object_;
};

// Hashes an NPObject so it can be used in a hash_map.
template <typename T>
class NPObjectPtrHash {
 public:
  size_t operator() (const NPObjectPtr<T>& ptr) const {
    return o3d::base::hash<size_t>()(reinterpret_cast<size_t>(ptr.Get()));
  }
};

// A V8 handle that automatically disposes itself when it is destroyed. There
// must be only one of these for each persistent handle, otherwise they might
// be disposed more than once.
template <typename T>
class AutoV8Persistent : public v8::Persistent<T> {
 public:
  AutoV8Persistent() {}

  template <typename U>
  explicit AutoV8Persistent(const v8::Persistent<U>& rhs)
      : v8::Persistent<T>(rhs) {
  }

  template <typename U>
  AutoV8Persistent& operator=(const v8::Persistent<U>& rhs) {
    *(v8::Persistent<T>*)this = rhs;
    return *this;
  }

  ~AutoV8Persistent() {
    this->Dispose();
    this->Clear();
  }
};

// The bridge provides a way of evaluating JavaScript in the V8 engine and
// marshalling between V8 and NPAPI representations of objects and values.
class NPV8Bridge {
  friend class NPV8Object;
 public:
  NPV8Bridge(ServiceLocator* service_locator, NPP npp);
  ~NPV8Bridge();

  NPP npp() { return npp_; }

  // Initializes the V8 environment. The global NPObject is wrapped with a V8
  // proxy and used as the global environment's prototype. This means that if
  // a variable cannot be resolved in the V8 environment then it will attempt
  // to resolve it in the NPObject. This allows V8 to read global variables in
  // the browser environment. Note that assignments will never go to the
  // global environment's prototype, changes will only be visible locally.
  void Initialize(const NPObjectPtr<NPObject>& global_np_object);

  // This function tells the bridge to forget and release all of the NPObjects
  // that it knows about.
  void ReleaseNPObjects();

  v8::Handle<v8::Context> script_context();

  // Evaluates some JavaScript code in V8. It currently expects only one
  // argument in the argument array, which must be a string containing the
  // JavaScript code to evaluate. It returns the result of the evaluation
  // as an NPAPI variant, which must be freed using NPN_ReleaseVariantValue.
  bool Evaluate(const NPVariant* np_args, int numArgs, NPVariant* np_result);

  // Adds an object property to the V8 global environment.
  void SetGlobalProperty(const String& name,
                         NPObjectPtr<NPObject>& np_object);

  // Converts a V8 value into an NPVariant. The NPVariant must be freed with
  // NPN_ReleaseVariantValue. Caller must enter the script context.
  NPVariant V8ToNPVariant(v8::Local<v8::Value> value);

  // Converts an NPVariant to a V8 value. Caller must enter the script context.
  v8::Local<v8::Value> NPToV8Variant(const NPVariant& np_variant);

  // Converts a V8 object to an NPObject, either by wrapping the V8 object
  // with an NPV8Object proxy or if the V8 object is a proxy, returning the
  // NPObject it wraps. Caller must enter the script context.
  NPObjectPtr<NPObject> V8ToNPObject(v8::Local<v8::Value> v8_object);

  // Converts an NPObject to a V8 object, either by wrapping the NPObject with
  // a V8 proxy or if the NPObject is a proxy, returning the V8 object it wraps.
  // Caller must enter the script context.
  v8::Local<v8::Value> NPToV8Object(const NPObjectPtr<NPObject>& np_object);

  // Determines whether the given NPObject is currently referenced by V8 through
  // a proxy.
  bool IsNPObjectReferenced(NPObjectPtr<NPObject> np_object);

 private:

  NPObjectPtr<NPObject> NPEvaluateObject(const char* script);

  void NPToV8Object(v8::Local<v8::Object> v8_target,
                    const NPObjectPtr<NPObject>& np_object);

  bool IsNPFunction(const NPObjectPtr<NPObject>& np_object);

  v8::Local<v8::Function> NPToV8Function(
      const NPObjectPtr<NPObject>& np_function);

  void ReleaseUnreferencedWrapperFunctions();

  NPObjectPtr<NPObject> WrapV8Function(const NPObjectPtr<NPObject>& np_object);

  void RegisterV8Object(v8::Local<v8::Object> v8_object,
                        const NPObjectPtr<NPObject>& np_object);

  void InitializeV8ObjectTemplate(
      v8::Local<v8::ObjectTemplate> v8_object_template);

  static void NPV8WeakReferenceCallback(v8::Persistent<v8::Value> value,
                                        void* parameter);

  void ReportV8Exception(const v8::TryCatch& tryCatch);

  v8::Local<v8::Array> NPToV8IdentifierArray(
      const NPVariant& np_array, bool named);

  v8::Local<v8::Array> NPToV8IdentifierArray(
      const NPIdentifier* ids, uint32_t id_count, bool named);

  // Implements enumeration of NPObject properties using NPN_Evaluate where
  // supported by the browser or otherwise falling back on emulation. Returns
  // either named or indexed properties depending on named parameter.
  v8::Local<v8::Array> Enumerate(const NPObjectPtr<NPObject> np_object,
                                 bool named);

  static v8::Handle<v8::Value> V8PropertyGetter(v8::Local<v8::Value> v8_name,
                                                const v8::AccessorInfo& info);

  static v8::Handle<v8::Value> V8PropertySetter(v8::Local<v8::Value> v8_name,
                                                v8::Local<v8::Value> v8_value,
                                                const v8::AccessorInfo& info);

  static v8::Handle<v8::Boolean> V8PropertyQuery(v8::Local<v8::Value> v8_name,
                                                 const v8::AccessorInfo& info);

  static v8::Handle<v8::Boolean> V8PropertyDeleter(
      v8::Local<v8::Value> v8_name,
      const v8::AccessorInfo& info);

  static v8::Handle<v8::Value> V8NamedPropertyGetter(
      v8::Local<v8::String> v8_name,
      const v8::AccessorInfo& info);

  static v8::Handle<v8::Value> V8NamedPropertySetter(
      v8::Local<v8::String> v8_name,
      v8::Local<v8::Value> v8_value,
      const v8::AccessorInfo& info);

  static v8::Handle<v8::Boolean> V8NamedPropertyQuery(
      v8::Local<v8::String> v8_name,
      const v8::AccessorInfo& info);

  static v8::Handle<v8::Boolean> V8NamedPropertyDeleter(
      v8::Local<v8::String> v8_name,
      const v8::AccessorInfo& info);

  static v8::Handle<v8::Array> V8NamedPropertyEnumerator(
      const v8::AccessorInfo& info);

  static v8::Handle<v8::Value> V8IndexedPropertyGetter(
      uint32_t index,
      const v8::AccessorInfo& info);

  static v8::Handle<v8::Value> V8IndexedPropertySetter(
      uint32_t index,
      v8::Local<v8::Value> v8_value,
      const v8::AccessorInfo& info);

  static v8::Handle<v8::Boolean> V8IndexedPropertyQuery(
      uint32_t index,
      const v8::AccessorInfo& info);

  static v8::Handle<v8::Boolean> V8IndexedPropertyDeleter(
      uint32_t index,
      const v8::AccessorInfo& info);

  static v8::Handle<v8::Array> V8IndexedPropertyEnumerator(
      const v8::AccessorInfo& info);

  static v8::Handle<v8::Value> V8CallNamedMethod(const v8::Arguments& args);

  static v8::Handle<v8::Value> V8CallFunction(const v8::Arguments& args);

  static v8::Handle<v8::Value> V8CallAsFunction(const v8::Arguments& args);

  NPObjectPtr<NPObject> GetNPConstructFunction(int arity);

  typedef o3d::base::hash_map<NPObjectPtr<NPObject>,
                              AutoV8Persistent<v8::Object>,
                              NPObjectPtrHash<NPObject> > NPV8ObjectMap;

  typedef std::map<int, NPObjectPtr<NPObject> > NPConstructFunctionMap;

  ServiceLocator* service_locator_;
  ServiceDependency<IErrorStatus> error_status_;
  NPP npp_;
  NPObjectPtr<NPObject> global_np_object_;
  AutoV8Persistent<v8::Context> script_context_;
  AutoV8Persistent<v8::FunctionTemplate> v8_np_constructor_template_;
  AutoV8Persistent<v8::Object> function_map_;
  AutoV8Persistent<v8::Object> global_prototype_;
  NPV8ObjectMap np_v8_object_map_;
  NPObjectPtr<NPObject> np_enumerate_function_;
  NPObjectPtr<NPObject> np_is_function_function_;
  NPObjectPtr<NPObject> np_wrap_function_function_;
  NPConstructFunctionMap np_construct_functions_;
  NPIdentifier np_name_identifier_;
  NPIdentifier np_call_identifier_;
  NPIdentifier np_length_identifier_;
  NPIdentifier np_proxy_identifier_;
  NPObjectPtr<NPObject> np_empty_array_;
  DISALLOW_COPY_AND_ASSIGN(NPV8Bridge);
};
}  // namespace o3d

#endif  // O3D_PLUGIN_CROSS_NP_V8_BRIDGE_H_
