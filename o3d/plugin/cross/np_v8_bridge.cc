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
// Tests are in o3d/tests/selenium/tests/v8.html. They can be run
// by opening the web page in a browser or as part of the selenium tests.

#include <npapi.h>
#include <sstream>
#include <vector>
#include "plugin/cross/np_v8_bridge.h"

using v8::AccessorInfo;
using v8::Arguments;
using v8::Array;
using v8::Context;
using v8::DontDelete;
using v8::DontEnum;
using v8::External;
using v8::Function;
using v8::FunctionTemplate;
using v8::HandleScope;
using v8::Int32;
using v8::Integer;
using v8::Local;
using v8::Message;
using v8::Null;
using v8::Number;
using v8::Object;
using v8::ObjectTemplate;
using v8::Persistent;
using v8::PropertyAttribute;
using v8::ReadOnly;
using v8::Script;
using v8::TryCatch;
using v8::Undefined;
using v8::Value;

namespace o3d {

// Only used during debugging. Type "o3d::DebugV8String(a.val_)" in the
// watch window to get the string representation of a V8 object.
const char* DebugV8String(Value* value) {
  static char buffer[4096];
  if (value == NULL) {
    ::base::snprintf(buffer, sizeof(buffer), "<null>");
  } else {
    value->ToString()->WriteUtf8(buffer);
  }
  return buffer;
}

namespace {

// The indices of the internal fields of a V8 proxy for an NPObject.
enum {
  // Pointer to the bridge that created the proxy.
  V8_NP_OBJECT_BRIDGE,
  // Pointer to the wrapped NPObject.
  V8_NP_OBJECT_WRAPPED,
  V8_NP_OBJECT_NUM_INTERNAL_FIELDS
};

// The name of the "hidden" property in a V8 non-proxy object that contains
// an External that points to the NPObject proxy for it. The property does
// not exist if there is no associated NPObject proxy.
const char* const kInternalProperty = "internal_property_";

// Convert an NPIdentifier (NULL, string or integer) to a V8 value.
Local<Value> NPToV8Identifier(NPIdentifier np_identifier) {
  if (np_identifier == NULL) {
    return Local<Value>();
  } else if (NPN_IdentifierIsString(np_identifier)) {
    NPUTF8* utf8_name = NPN_UTF8FromIdentifier(np_identifier);
    Local<v8::String> v8_identifier = v8::String::New(utf8_name);
    NPN_MemFree(utf8_name);
    return v8_identifier;
  } else {
    return Integer::New(NPN_IntFromIdentifier(np_identifier));
  }
}

// Convert a V8 value (empty, string or integer) into an NPIdentifier.
NPIdentifier V8ToNPIdentifier(v8::Handle<Value> v8_identifier) {
  if (v8_identifier.IsEmpty()) {
    return NULL;
  } else if (v8_identifier->IsNumber()) {
    return NPN_GetIntIdentifier(v8_identifier->Int32Value());
  } else if (v8_identifier->IsString()) {
    return NPN_GetStringIdentifier(
        *v8::String::Utf8Value(v8_identifier->ToString()));
  } else {
    return NULL;
  }
}
}  // namespace anonymous

// The class of NPObject proxies that wrap V8 objects. These field the NPAPI
// functions and translate them into V8 calls.
class NPV8Object : public NPObject {
 public:
  static NPObjectPtr<NPV8Object> Create(NPV8Bridge* bridge,
                                        Local<Object> v8_object) {
    NPObjectPtr<NPV8Object> np_object =
        NPObjectPtr<NPV8Object>::AttachToReturned(
            static_cast<NPV8Object*>(NPN_CreateObject(bridge->npp(),
                                                      &np_class_)));
    np_object->v8_object_ = Persistent<Object>::New(v8_object);
    np_object->bridge_ = bridge;
    return np_object;
  }

  v8::Handle<Object> v8_object() const {
    return v8_object_;
  }

  // Drop references between NPObject and V8 object. Must be called before the
  // NPObject is destroyed so V8 can garbage collect the associated V8 object.
  void UnlinkFromV8() {
    HandleScope handle_scope;
    if (!v8_object_.IsEmpty()) {
      v8_object_->DeleteHiddenValue(v8::String::NewSymbol(kInternalProperty));
      v8_object_.Dispose();
      v8_object_.Clear();
    }
  }

  static NPClass np_class_;

 private:
  NPV8Object() : bridge_(NULL) {
  }

  static NPObject* Allocate(NPP npp, NPClass* np_class) {
    NPV8Object* np_v8_object = new NPV8Object();
    np_v8_object->bridge_ = NULL;
    return np_v8_object;
  }

  static void Deallocate(NPObject* np_object) {
    NPV8Object* np_v8_object = static_cast<NPV8Object*> (np_object);
    // Uncomment this line to see objects with a non-zero reference
    // count being deallocated. For example, Firefox does this when unloading
    // the plugin.
    // DCHECK_EQ(0, np_v8_object_map->referenceCount);
    np_v8_object->UnlinkFromV8();
    delete np_v8_object;
  }

  static void Invalidate(NPObject* np_object) {
    NPV8Object* np_v8_object = static_cast<NPV8Object*> (np_object);
    np_v8_object->bridge_ = NULL;
    np_v8_object->UnlinkFromV8();
  }

  static bool HasMethod(NPObject* np_object, NPIdentifier np_name) {
    NPV8Object* np_v8_object = static_cast<NPV8Object*> (np_object);
    NPV8Bridge* bridge = np_v8_object->bridge_;
    if (bridge == NULL)
      return false;

    HandleScope handle_scope;
    Context::Scope scope(bridge->script_context());
    TryCatch tryCatch;

    v8::Handle<Object> v8_object = np_v8_object->v8_object_;
    if (v8_object.IsEmpty())
      return false;

    Local<Value> v8_name = NPToV8Identifier(np_name);
    Local<Value> value = v8_object->Get(v8_name);
    if (tryCatch.HasCaught()) {
      bridge->ReportV8Exception(tryCatch);
      return false;
    }

    // Returns true iff the object has a property with the given name and
    // the object assigned to the property is a function. This works for V8
    // functions and assigned browser JavaScript functions (because their
    // proxies are created from FunctionTemplates so V8 considers them to be
    // functions).
    return !value.IsEmpty() && value->IsFunction();
  }

  // Called when a method is invoked through "obj.m(...)".
  static bool Invoke(NPObject* np_object, NPIdentifier np_name,
                     const NPVariant* np_args, uint32_t numArgs,
                     NPVariant* result) {
    // This works around a bug in Chrome:
    // http://code.google.com/p/chromium/issues/detail?id=5110
    // NPN_InvokeDefault is transformed into a call to Invoke on the plugin with
    // a null method name identifier.
    if (np_name == NULL) {
      return InvokeDefault(np_object, np_args, numArgs, result);
    }

    NPV8Object* np_v8_object = static_cast<NPV8Object*> (np_object);
    NPV8Bridge* bridge = np_v8_object->bridge_;
    if (bridge == NULL)
      return false;

    HandleScope handle_scope;
    Context::Scope scope(bridge->script_context());
    TryCatch tryCatch;

    v8::Handle<Object> v8_object = np_v8_object->v8_object_;
    if (v8_object.IsEmpty())
      return false;

    Local<Value> v8_name = NPToV8Identifier(np_name);
    Local<Value> value = v8_object->Get(v8_name);
    if (value.IsEmpty() || !value->IsFunction())
      return false;
    Local<Function> function = Local<Function>::Cast(value);
    std::vector<v8::Handle<Value> > v8_args(numArgs);
    for (uint32_t i = 0; i != numArgs; ++i) {
      v8_args[i] = bridge->NPToV8Variant(np_args[i]);
    }

    *result = bridge->V8ToNPVariant(
        function->Call(v8_object, numArgs,
                       numArgs == 0 ? NULL : &v8_args.front()));
    if (tryCatch.HasCaught()) {
      bridge->ReportV8Exception(tryCatch);
      return false;
    }
    return true;
  }

  // Called when an object is called as a function "f(...)".
  static bool InvokeDefault(NPObject* np_object, const NPVariant* np_args,
                            uint32_t numArgs, NPVariant* result) {
    NPV8Object* np_v8_object = static_cast<NPV8Object*> (np_object);
    NPV8Bridge* bridge = np_v8_object->bridge_;
    if (bridge == NULL)
      return false;

    HandleScope handle_scope;
    Context::Scope scope(bridge->script_context());
    TryCatch tryCatch;

    v8::Handle<Object> v8_object = np_v8_object->v8_object_;
    if (v8_object.IsEmpty())
      return false;

    if (!v8_object->IsFunction())
      return false;
    v8::Handle<Function> function = v8::Handle<Function>::Cast(v8_object);

    std::vector<v8::Handle<Value> > v8_args(numArgs);
    for (uint32_t i = 0; i != numArgs; ++i) {
      v8_args[i] = bridge->NPToV8Variant(np_args[i]);
    }

    *result = bridge->V8ToNPVariant(
        function->Call(v8_object, numArgs,
                       numArgs == 0 ? NULL : &v8_args.front()));
    if (tryCatch.HasCaught()) {
      bridge->ReportV8Exception(tryCatch);
      return false;
    }
    return true;
  }

  // Called when an object is called as a constructor "new C(...)".
  static bool Construct(NPObject* np_object, const NPVariant* np_args,
                        uint32_t numArgs, NPVariant* result) {
    NPV8Object* np_v8_object = static_cast<NPV8Object*> (np_object);
    NPV8Bridge* bridge = np_v8_object->bridge_;
    if (bridge == NULL)
      return false;

    HandleScope handle_scope;
    Context::Scope scope(bridge->script_context());
    TryCatch tryCatch;

    v8::Handle<Object> v8_object = np_v8_object->v8_object_;
    if (v8_object.IsEmpty())
      return false;

    if (!v8_object->IsFunction())
      return false;
    v8::Handle<Function> function = v8::Handle<Function>::Cast(v8_object);

    std::vector<v8::Handle<Value> > v8_args(numArgs);
    for (uint32_t i = 0; i != numArgs; ++i) {
      v8_args[i] = bridge->NPToV8Variant(np_args[i]);
    }

    Local<Object> v8_result = function->NewInstance(
        numArgs, numArgs == 0 ? NULL : &v8_args.front());
    if (v8_result.IsEmpty())
      return false;

    *result = bridge->V8ToNPVariant(v8_result);
    if (tryCatch.HasCaught()) {
      bridge->ReportV8Exception(tryCatch);
      return false;
    }
    return true;
  }

  static bool HasProperty(NPObject* np_object, NPIdentifier np_name) {
    NPV8Object* np_v8_object = static_cast<NPV8Object*> (np_object);
    NPV8Bridge* bridge = np_v8_object->bridge_;
    if (bridge == NULL)
      return false;

    HandleScope handle_scope;
    Context::Scope scope(bridge->script_context());

    v8::Handle<Object> v8_object = np_v8_object->v8_object_;
    if (v8_object.IsEmpty())
      return false;

    // This is a better approach than the one below. It allows functions
    // to be retreived as first class objects. Unfortunately we can't
    // support this yet because of a Chrome bug:
    // http://code.google.com/p/chromium/issues/detail?id=5742
    // if (NPN_IdentifierIsString(np_name)) {
    //   Local<v8::String> v8_name = Local<v8::String>::Cast(
    //       NPToV8Identifier(np_name));
    //   return v8_object->Has(v8_name);
    // } else {
    //   return v8_object->Has(NPN_IntFromIdentifier(np_name));
    // }

    // Instead hide properties with function type. This ensures that Chrome
    // will invoke them with Invoke rather than InvokeDefault. The problem
    // with InvokeDefault is it doesn't tell us what "this" should be
    // bound to, whereas Invoke does.
    Local<Value> v8_name = NPToV8Identifier(np_name);
    if (NPN_IdentifierIsString(np_name)) {
      if (!v8_object->Has(v8_name->ToString())) {
        return false;
      }
    } else {
      if (!v8_object->Has(NPN_IntFromIdentifier(np_name))) {
        return false;
      }
    }
    Local<Value> v8_property_value = v8_object->Get(v8_name);
    if (v8_property_value->IsFunction()) {
      return false;
    }

    return true;
  }

  static bool GetProperty(NPObject* np_object, NPIdentifier np_name,
                          NPVariant* result) {
    NPV8Object* np_v8_object = static_cast<NPV8Object*> (np_object);
    NPV8Bridge* bridge = np_v8_object->bridge_;
    if (bridge == NULL)
      return false;

    HandleScope handle_scope;
    Context::Scope scope(bridge->script_context());
    TryCatch tryCatch;

    v8::Handle<Object> v8_object = np_v8_object->v8_object_;
    if (v8_object.IsEmpty())
      return false;

    Local<Value> v8_name = NPToV8Identifier(np_name);
    Local<Value> v8_property_value = v8_object->Get(v8_name);
    if (tryCatch.HasCaught()) {
      bridge->ReportV8Exception(tryCatch);
      return false;
    }

    // See comment in HasProperty. Do not return properties that are
    // functions. It will prevent Chrome from invoking them as methods.
    if (v8_property_value.IsEmpty() || v8_property_value->IsFunction())
      return false;

    *result = bridge->V8ToNPVariant(v8_property_value);
    return true;
  }

  static bool SetProperty(NPObject* np_object, NPIdentifier np_name,
                          const NPVariant* np_value) {
    NPV8Object* np_v8_object = static_cast<NPV8Object*> (np_object);
    NPV8Bridge* bridge = np_v8_object->bridge_;
    if (bridge == NULL)
      return false;

    HandleScope handle_scope;
    Context::Scope scope(bridge->script_context());
    TryCatch tryCatch;

    v8::Handle<Object> v8_object = np_v8_object->v8_object_;
    if (v8_object.IsEmpty())
      return false;

    Local<Value> v8_name = NPToV8Identifier(np_name);
    bool success = v8_object->Set(v8_name, bridge->NPToV8Variant(*np_value));

    if (tryCatch.HasCaught()) {
      bridge->ReportV8Exception(tryCatch);
      return false;
    }

    return success;
  }

  static bool RemoveProperty(NPObject* np_object, NPIdentifier np_name) {
    NPV8Object* np_v8_object = static_cast<NPV8Object*> (np_object);
    NPV8Bridge* bridge = np_v8_object->bridge_;
    if (bridge == NULL)
      return false;

    HandleScope handle_scope;
    Context::Scope scope(bridge->script_context());
    TryCatch tryCatch;

    v8::Handle<Object> v8_object = np_v8_object->v8_object_;
    if (v8_object.IsEmpty())
      return false;

    bool success;
    if (NPN_IdentifierIsString(np_name)) {
      NPUTF8* utf8_name = NPN_UTF8FromIdentifier(np_name);
      Local<v8::String> v8_name = v8::String::New(utf8_name);
      NPN_MemFree(utf8_name);
      success = v8_object->Delete(v8_name);
    } else {
      success = v8_object->Delete(NPN_IntFromIdentifier(np_name));
    }

    if (tryCatch.HasCaught()) {
      bridge->ReportV8Exception(tryCatch);
      return false;
    }

    return success;
  }

  static bool Enumerate(NPObject* np_object, NPIdentifier** np_names,
                        uint32_t* numNames) {
    NPV8Object* np_v8_object = static_cast<NPV8Object*> (np_object);
    NPV8Bridge* bridge = np_v8_object->bridge_;
    if (bridge == NULL)
      return false;

    HandleScope handle_scope;
    Context::Scope scope(bridge->script_context());

    v8::Handle<Object> v8_object = np_v8_object->v8_object_;
    if (v8_object.IsEmpty())
      return false;

    Local<Array> v8_names = v8_object->GetPropertyNames();

    // Due to a bug in Chrome, need to filter out any properties that
    // are functions. See comment in HasProperty.
    int num_non_function_properties = 0;
    for (int i = 0; i != v8_names->Length(); ++i) {
      Local<Value> v8_property_value =
          v8_object->Get(v8_names->Get(Int32::New(i)));
      if (!v8_property_value->IsFunction()) {
        ++num_non_function_properties;
      }
    }
    *numNames = num_non_function_properties;
    *np_names = static_cast<NPIdentifier*> (
        NPN_MemAlloc(num_non_function_properties * sizeof(NPIdentifier)));
    int j = 0;
    for (uint32_t i = 0; i != v8_names->Length(); ++i) {
      Local<Value> v8_name = v8_names->Get(Int32::New(i));
      Local<Value> v8_property_value = v8_object->Get(v8_name);
      if (!v8_property_value->IsFunction()) {
        (*np_names)[j++] = V8ToNPIdentifier(v8_name);
      }
    }

    return true;
  }

  NPV8Bridge* bridge_;
  AutoV8Persistent<Object> v8_object_;
  DISALLOW_COPY_AND_ASSIGN(NPV8Object);
};

NPClass NPV8Object::np_class_ = {
  NP_CLASS_STRUCT_VERSION,
  Allocate,
  Deallocate,
  Invalidate,
  HasMethod,
  Invoke,
  InvokeDefault,
  HasProperty,
  GetProperty,
  SetProperty,
  RemoveProperty,
  Enumerate,
  Construct
};

NPV8Bridge::NPV8Bridge(ServiceLocator* service_locator, NPP npp)
    : service_locator_(service_locator),
      error_status_(service_locator),
      npp_(npp) {
  np_name_identifier_ = NPN_GetStringIdentifier("name");
  np_call_identifier_ = NPN_GetStringIdentifier("call");
  np_length_identifier_ = NPN_GetStringIdentifier("length");
  np_proxy_identifier_ = NPN_GetStringIdentifier("npv8_proxy_");
}

NPV8Bridge::~NPV8Bridge() {
  // Do not call weak reference callback after the bridge is destroyed
  // because the callbacks assume it exists. The only purpose of the callback
  // is to remove the corresponding object entry from the NP-V8 object map
  // and its about to get cleared anyway.
  for (NPV8ObjectMap::iterator it = np_v8_object_map_.begin();
       it != np_v8_object_map_.end(); ++it) {
    it->second.ClearWeak();
  }
}

NPObjectPtr<NPObject> NPV8Bridge::NPEvaluateObject(const char* script) {
  NPString np_script = { script, strlen(script) };
  NPVariant np_variant;
  NPObjectPtr<NPObject> np_result;
  if (NPN_Evaluate(npp_, global_np_object_.Get(), &np_script, &np_variant)) {
    if (NPVARIANT_IS_OBJECT(np_variant)) {
      np_result = NPObjectPtr<NPObject>(NPVARIANT_TO_OBJECT(np_variant));
    }
    NPN_ReleaseVariantValue(&np_variant);
  }
  return np_result;
}

namespace {
// Create code that looks like this:
// (function(func, protoArray) {
//   return function() {
//     switch (arguments.length) {
//     case 0:
//       return func.call(this);
//     case 1:
//       return func.call(this,
//                        arguments[0]);
//     case 2:
//       return func.call(this,
//                        arguments[0],
//                        arguments[1]);
// ...
//     default:
//       var args = protoArray.slice();
//       for (var i = 0; i < arguments.length; ++i) {
//         args[i] = arguments[i];
//       }
//       return func.apply(this, args);
//     }
//   };
// })
String MakeWrapFunctionScript() {
  std::ostringstream code;
  code << "(function(func, protoArray) {";
  code << "  return function() {";
  code << "    switch (arguments.length) {";
  for (int i = 0; i <= 10; ++i) {
    code << "  case " << i << ": return func.call(this";
    for (int j = 0; j < i; ++j) {
      code << ", arguments[" << j << "]";
    }
    code << ");";
  }
  code << "    default:";
  code << "      var args = protoArray.slice();";
  code << "      for (var i = 0; i < arguments.length; ++i) {";
  code << "        args.push(arguments[i]);";
  code << "      }";
  code << "      return func.apply(this, args);";
  code << "    }";
  code << "  };";
  code << "})";
  return code.str();
}
}  // namespace anonymous

void NPV8Bridge::Initialize(const NPObjectPtr<NPObject>& global_np_object) {
  HandleScope handle_scope;

  global_np_object_ = global_np_object;

  // This template is used for V8 proxies of NPObjects.
  v8_np_constructor_template_ = Persistent<FunctionTemplate>::New(
      FunctionTemplate::New());
  InitializeV8ObjectTemplate(v8_np_constructor_template_->InstanceTemplate());

  // This template is used for the global V8 object.
  Local<FunctionTemplate> v8_global_template = FunctionTemplate::New();
  InitializeV8ObjectTemplate(v8_global_template->PrototypeTemplate());

  script_context_ = Context::New(NULL, v8_global_template->InstanceTemplate());
  Context::Scope scope(script_context_);

  // Give the global object a prototype that allows V8 to access global
  // variables in another JavaScript environemnt over NPAPI.
  Local<Object> v8_global_prototype =
      Local<Object>::Cast(script_context_->Global()->GetPrototype());
  Local<Object> v8_global_prototype2 =
      Local<Object>::Cast(v8_global_prototype->GetPrototype());
  global_prototype_ = Persistent<Object>::New(v8_global_prototype2);
  NPToV8Object(v8_global_prototype2, global_np_object);

  function_map_ = Persistent<Object>::New(Object::New());

  // Create a browser JavaScript function that can later be called to get the
  // type of an object (as the browser sees it). This is useful for determining
  // whether an object received over NPAPI is a function (which means its
  // proxy must be created from a FunctionTemplate rather than an
  // ObjectTemplate).
  static const char kIsFunctionScript[] =
      "(function(obj) { return obj instanceof Function; })";
  np_is_function_function_ = NPEvaluateObject(kIsFunctionScript);

  // Create a browser JavaScript function that can later be used to enumerate
  // the properties of an object. This is used as a fallback if NPN_Evaluate
  // is not implemented by the browser (like Firefox 2) and the enumerate
  // callback is not implemented by the NPObject.
  static const char kEnumerateScript[] =
      "(function(object) {"
      "  var properties = [];"
      "  for (var property in object) {"
      "    if (object.hasOwnProperty(property)) {"
      "      properties[properties.length++] = property;"
      "    }"
      "  }"
      "  return properties;"
      "})";
  np_enumerate_function_ = NPEvaluateObject(kEnumerateScript);

  // Create a browser JavaScript function that can later be used to create
  // a wrapper around an V8 function proxy, making it appear to be a real
  // browser function.
  np_wrap_function_function_ = NPEvaluateObject(
      MakeWrapFunctionScript().c_str());

  // Create an NPObject proxy for a V8 array. This is for the browser to use as
  // a prototype for creating new V8 arrays with slice().
  np_empty_array_ = V8ToNPObject(v8::Array::New(0));
}

void NPV8Bridge::ReleaseNPObjects() {
  np_v8_object_map_.clear();
  np_construct_functions_.clear();

  global_np_object_.Clear();
  np_is_function_function_.Clear();
  np_enumerate_function_.Clear();
  np_wrap_function_function_.Clear();
  np_empty_array_.Clear();
}

v8::Handle<Context> NPV8Bridge::script_context() {
  return script_context_;
}

bool NPV8Bridge::Evaluate(const NPVariant* np_args, int numArgs,
                          NPVariant* np_result) {
  HandleScope handle_scope;
  Context::Scope scope(script_context_);

  Local<Value> v8_code;
  if (numArgs == 1) {
    v8_code = NPToV8Variant(np_args[0]);
  } else {
    return false;
  }

  if (v8_code.IsEmpty() || !v8_code->IsString())
    return false;

  TryCatch tryCatch;

  Local<Script> v8_script = v8::Script::Compile(v8_code->ToString());
  if (tryCatch.HasCaught()) {
    ReportV8Exception(tryCatch);
    return false;
  }
  if (v8_script.IsEmpty())
    return false;

  Local<Value> v8_result = v8_script->Run();
  if (tryCatch.HasCaught()) {
    ReportV8Exception(tryCatch);
    return false;
  }
  if (v8_result.IsEmpty())
    return false;

  *np_result = V8ToNPVariant(v8_result);
  return true;
}

void NPV8Bridge::SetGlobalProperty(const String& name,
                                   NPObjectPtr<NPObject>& np_object) {
  HandleScope handle_scope;
  Context::Scope scope(script_context_);
  script_context_->Global()->Set(v8::String::New(name.c_str()),
                                 NPToV8Object(np_object));
}

NPVariant NPV8Bridge::V8ToNPVariant(Local<Value> value) {
  NPVariant np_variant;
  if (value.IsEmpty() || value->IsUndefined()) {
    VOID_TO_NPVARIANT(np_variant);
  } else if (value->IsNull()) {
    NULL_TO_NPVARIANT(np_variant);
  } else if (value->IsBoolean()) {
    BOOLEAN_TO_NPVARIANT(value->BooleanValue(), np_variant);
  } else if (value->IsInt32()) {
    INT32_TO_NPVARIANT(value->Int32Value(), np_variant);
  } else if (value->IsNumber()) {
    DOUBLE_TO_NPVARIANT(value->NumberValue(), np_variant);
  } else if (value->IsString()) {
    Local<v8::String> v8_string = value->ToString();
    int utf8_length = v8_string->Length();
    NPUTF8* utf8_chars = static_cast<NPUTF8*>(NPN_MemAlloc(utf8_length + 1));
    v8_string->WriteUtf8(utf8_chars);
    STRINGN_TO_NPVARIANT(utf8_chars, utf8_length, np_variant);
  } else if (value->IsObject()) {
    Local<Object> v8_object = value->ToObject();
    NPObjectPtr<NPObject> np_object = V8ToNPObject(v8_object);
    OBJECT_TO_NPVARIANT(np_object.Disown(), np_variant);
  }
  return np_variant;
}

Local<Value> NPV8Bridge::NPToV8Variant(const NPVariant& np_variant) {
  Local<Value> v8_result;
  switch (np_variant.type) {
    case NPVariantType_Void:
      v8_result = Local<Value>::New(Undefined());
      break;
    case NPVariantType_Null:
      v8_result = Local<Value>::New(Null());
      break;
    case NPVariantType_Bool:
      v8_result = Local<Value>::New(
          v8::Boolean::New(NPVARIANT_TO_BOOLEAN(np_variant)));
      break;
    case NPVariantType_Int32:
      v8_result = Local<Value>::New(
          Int32::New(NPVARIANT_TO_INT32(np_variant)));
      break;
    case NPVariantType_Double:
      v8_result = Local<Value>::New(
          Number::New(NPVARIANT_TO_DOUBLE(np_variant)));
      break;
    case NPVariantType_String:
      {
        NPString np_string = NPVARIANT_TO_STRING(np_variant);
        v8_result = Local<Value>::New(
            v8::String::New(np_string.utf8characters, np_string.utf8length));
        break;
      }
    case NPVariantType_Object:
      v8_result = NPToV8Object(
          NPObjectPtr<NPObject>(NPVARIANT_TO_OBJECT(np_variant)));
      break;
    default:
      v8_result = Local<Value>();
      break;
  }
  return v8_result;
}

NPObjectPtr<NPObject> NPV8Bridge::V8ToNPObject(Local<Value> v8_value) {
  NPObjectPtr<NPObject> np_object;
  if (!v8_value.IsEmpty() && v8_value->IsObject()) {
    Local<Object> v8_object = Local<Object>::Cast(v8_value);
    if (v8_object->InternalFieldCount() == 0) {
      // It is must be a V8 created JavaScript object (or function), a V8
      // function proxy for an NP function or a V8 function proxy for a named
      // native method. If it is already associated with an NP object then that
      // will be stored in the "internal property". Return that if it's there,
      // otherwise create a new NP proxy.
      Local<v8::String> internal_name = v8::String::NewSymbol(
          kInternalProperty);
      Local<Value> v8_internal = v8_object->GetHiddenValue(internal_name);

      if (v8_internal.IsEmpty() || v8_internal->IsUndefined()) {
        // No existing NP object so create a proxy and store it in the "internal
        // property".
        np_object = NPV8Object::Create(this, v8_object);
        v8_internal = External::New(np_object.Get());
        v8_object->SetHiddenValue(internal_name, v8_internal);
      } else {
        np_object = NPObjectPtr<NPObject>(
            static_cast<NPObject*>(
                Local<External>::Cast(v8_internal)->Value()));
      }

      // If it is a V8 function then wrap it in a browser function so that its
      // typeof will be reported as 'function' in the browser and it can be
      // used in cases where a real function is required (rather than an
      // object that just happens to be invocable.
      if (v8_value->IsFunction() &&
          np_object->_class == &NPV8Object::np_class_) {
        np_object = WrapV8Function(np_object);
      }
    } else {
      // This is a V8 object proxy. The NP object is referenced from an internal
      // field.
      Local<Value> internal = v8_object->GetInternalField(
          V8_NP_OBJECT_WRAPPED);
      np_object = NPObjectPtr<NPObject>(
          static_cast<NPObject*>(Local<External>::Cast(internal)->Value()));
    }
  }
  return np_object;
}

// Wrap NPV8Object proxying a V8 function in a browser function so that its
// typeof will be reported as 'function' in the browser and it can be
// used in cases where a real function is required (rather than an
// object that just happens to be invocable.
// A new wrapper function is created whenever a V8 function crosses into the
// browser. So === won't do the right thing in the browser.
NPObjectPtr<NPObject> NPV8Bridge::WrapV8Function(
    const NPObjectPtr<NPObject>& np_object) {

  NPObjectPtr<NPObject> np_result = np_object;
  NPVariant np_args[2];
  OBJECT_TO_NPVARIANT(np_object.Get(), np_args[0]);
  OBJECT_TO_NPVARIANT(np_empty_array_.Get(), np_args[1]);
  NPVariant np_variant;
  if (NPN_InvokeDefault(npp_, np_wrap_function_function_.Get(),
                        np_args, 2, &np_variant)) {
    if (NPVARIANT_IS_OBJECT(np_variant)) {
      NPObjectPtr<NPObject> np_wrapper(NPVARIANT_TO_OBJECT(np_variant));

      // Add a reference back to the NPV8Object so we can find it again.
      if (NPN_SetProperty(npp_, np_wrapper.Get(), np_proxy_identifier_,
                          &np_args[0])) {
        np_result = np_wrapper;
      }
    }
    NPN_ReleaseVariantValue(&np_variant);
  }
  return np_result;
}

Local<Value> NPV8Bridge::NPToV8Object(const NPObjectPtr<NPObject>& np_object) {
  if (np_object.IsNull())
    return Local<Value>::New(Null());

  // This might be a wrapper for a function. Find the actual proxy in that
  // case.
  NPObjectPtr<NPObject> np_real_object = np_object;
  {
    // NPN_GetProperty might cause an O3D NPObject to set an error if the
    // property does not exist. Prevent that. It would be better to simply
    // test whether the property exists by calling NPN_HasProperty but that
    // is not supported in Mac Safari.
    ErrorSuppressor error_suppressor(service_locator_);
    NPVariant np_variant;
    if (NPN_GetProperty(npp_, np_real_object.Get(), np_proxy_identifier_,
                        &np_variant)) {
      if (NPVARIANT_IS_OBJECT(np_variant)) {
        np_real_object = NPVARIANT_TO_OBJECT(np_variant);
      }
      NPN_ReleaseVariantValue(&np_variant);
    }
  }

  if (np_real_object->_class == &NPV8Object::np_class_) {
    NPV8Object* np_v8_object = static_cast<NPV8Object*>(np_real_object.Get());
    return Local<Object>::New(np_v8_object->v8_object());
  } else {
    NPV8ObjectMap::const_iterator it = np_v8_object_map_.find(np_real_object);
    if (it != np_v8_object_map_.end())
      return Local<Object>::New(it->second);

    if (IsNPFunction(np_real_object)) {
      return NPToV8Function(np_real_object);
    } else {
      Local<Function> v8_function = v8_np_constructor_template_->GetFunction();
      Local<Object> v8_object = v8_function->NewInstance();
      if (!v8_object.IsEmpty()) {
        // NewInstance sets a JavaScript exception if it fails. Eventually
        // it'll be caught when control flow hits a TryCatch. Just make sure
        // not to dereference it before then.
        NPToV8Object(v8_object, np_real_object);
      }
      return v8_object;
    }
  }
}

void NPV8Bridge::NPToV8Object(v8::Local<Object> v8_target,
                              const NPObjectPtr<NPObject>& np_object) {
  v8_target->SetInternalField(V8_NP_OBJECT_BRIDGE, External::New(this));
  v8_target->SetInternalField(V8_NP_OBJECT_WRAPPED,
                              External::New(np_object.Get()));
  RegisterV8Object(v8_target, np_object);
}

bool NPV8Bridge::IsNPFunction(const NPObjectPtr<NPObject>& np_object) {
  // Before invoking the potentially expensive instanceof function (it has to
  // go through the browser) check whether the object has a call
  // property. If it doesn't have one then it isn't a JavaScript
  // function.
  if (!NPN_HasProperty(npp_, np_object.Get(), np_call_identifier_)) {
    return false;
  }

  // If it looks like it might be a function then call the instanceof function
  // in the browser to confirm.
  bool is_function = false;
  NPVariant np_object_variant;
  OBJECT_TO_NPVARIANT(np_object.Get(), np_object_variant);
  NPVariant np_is_function;
  if (NPN_InvokeDefault(npp_, np_is_function_function_.Get(),
                        &np_object_variant, 1, &np_is_function)) {
    if (NPVARIANT_IS_BOOLEAN(np_is_function)) {
      is_function = NPVARIANT_TO_BOOLEAN(np_is_function);
    }
    NPN_ReleaseVariantValue(&np_is_function);
  }
  return is_function;
}

v8::Local<v8::Function> NPV8Bridge::NPToV8Function(
    const NPObjectPtr<NPObject>& np_function) {
  Local<FunctionTemplate> v8_function_template = FunctionTemplate::New(
      V8CallFunction, External::New(this));

  Local<Function> v8_function = v8_function_template->GetFunction();

  Local<v8::String> internal_name = v8::String::NewSymbol(
      kInternalProperty);
  v8_function->SetHiddenValue(internal_name, External::New(np_function.Get()));

  // Copy function name from NP function.
  NPVariant np_name;
  if (NPN_GetProperty(npp_, np_function.Get(), np_name_identifier_, &np_name)) {
    Local<Value> v8_name_value = NPToV8Variant(np_name);
    NPN_ReleaseVariantValue(&np_name);
    if (!v8_name_value.IsEmpty() && v8_name_value->IsString()) {
      Local<v8::String> v8_name = Local<v8::String>::Cast(v8_name_value);
      v8_function->SetName(v8_name);
    }
  }

  RegisterV8Object(v8_function, np_function);
  return v8_function;
}

void NPV8Bridge::RegisterV8Object(v8::Local<v8::Object> v8_object,
                                  const NPObjectPtr<NPObject>& np_object) {
  np_v8_object_map_[np_object] = Persistent<Object>::New(v8_object);
  np_v8_object_map_[np_object].MakeWeak(this, NPV8WeakReferenceCallback);
}

bool NPV8Bridge::IsNPObjectReferenced(NPObjectPtr<NPObject> np_object) {
  return np_v8_object_map_.find(np_object) != np_v8_object_map_.end();
}

void NPV8Bridge::InitializeV8ObjectTemplate(
    Local<ObjectTemplate> v8_object_template) {
  v8_object_template->SetInternalFieldCount(
      V8_NP_OBJECT_NUM_INTERNAL_FIELDS);
  v8_object_template->SetNamedPropertyHandler(V8NamedPropertyGetter,
                                              V8NamedPropertySetter,
                                              V8NamedPropertyQuery,
                                              V8NamedPropertyDeleter,
                                              V8NamedPropertyEnumerator);
  v8_object_template->SetIndexedPropertyHandler(V8IndexedPropertyGetter,
                                                V8IndexedPropertySetter,
                                                V8IndexedPropertyQuery,
                                                V8IndexedPropertyDeleter,
                                                V8IndexedPropertyEnumerator);
  v8_object_template->SetCallAsFunctionHandler(V8CallAsFunction);
}

void NPV8Bridge::NPV8WeakReferenceCallback(Persistent<Value> v8_value,
                                           void* parameter) {
  HandleScope handle_scope;
  NPV8Bridge* bridge = static_cast<NPV8Bridge*>(parameter);
  NPObjectPtr<NPObject> np_object = bridge->V8ToNPObject(
      Local<Value>::New(v8_value));
  bridge->np_v8_object_map_.erase(np_object);
}

void NPV8Bridge::ReportV8Exception(const TryCatch& v8_try_catch) {
  if (v8_try_catch.HasCaught()) {
    Local<Message> v8_message = v8_try_catch.Message();
    if (v8_message.IsEmpty()) {
      Local<Value> v8_exception = v8_try_catch.Exception();
      if (v8_exception.IsEmpty()) {
        error_status_->SetLastError(
            "An unknown exception ocurred while executing V8 JavaScript code");
      } else {
        v8::String::Utf8Value as_utf8(v8_exception);
        if (*as_utf8) {
          error_status_->SetLastError(*as_utf8);
        } else {
          error_status_->SetLastError(
              "An exception was thrown but its toString method failed");
        }
      }
    } else {
      String source_line(*v8::String::Utf8Value(v8_message->GetSourceLine()));
      String text(*v8::String::Utf8Value(v8_message->Get()));
      String message = text + " in " + source_line;
      error_status_->SetLastError(message);
    }
  }
}

v8::Local<v8::Array> NPV8Bridge::NPToV8IdentifierArray(
    const NPVariant& np_array, bool named) {
  Local<Array> v8_array;
  if (!NPVARIANT_IS_OBJECT(np_array))
    return v8_array;

  NPObject* np_array_object = NPVARIANT_TO_OBJECT(np_array);
  NPVariant np_length;
  if (NPN_GetProperty(npp_, np_array_object, np_length_identifier_,
                      &np_length)) {
    Local<Value> v8_length = NPToV8Variant(np_length);
    NPN_ReleaseVariantValue(&np_length);

    if (v8_length.IsEmpty() || !v8_length->IsNumber())
      return v8_array;

    int length = v8_length->Int32Value();
    Local<Array> v8_untrimmed_array = Array::New(length);
    int num_elements = 0;
    for (int i = 0; i < length; ++i) {
      NPVariant np_element;
      if (!NPN_GetProperty(npp_, np_array_object, NPN_GetIntIdentifier(i),
                           &np_element))
        return Local<Array>();
      Local<Value> v8_element = NPToV8Variant(np_element);
      NPN_ReleaseVariantValue(&np_element);
      if (v8_element->IsString() == named) {
        v8_untrimmed_array->Set(Int32::New(num_elements), v8_element);
        ++num_elements;
      }
    }
    v8_array = Array::New(num_elements);
    for (int i = 0; i < num_elements; ++i) {
      Local<Integer> i_handle = Integer::New(i);
      v8_array->Set(i_handle, v8_untrimmed_array->Get(i_handle));
    }
  }

  return v8_array;
}

Local<Array> NPV8Bridge::NPToV8IdentifierArray(const NPIdentifier* ids,
                                               uint32_t id_count, bool named) {
  int num_elements = 0;
  for (uint32_t i = 0; i < id_count; ++i) {
    if (NPN_IdentifierIsString(ids[i]) == named) {
      ++num_elements;
    }
  }
  Local<Array> v8_array = Array::New(num_elements);
  int j = 0;
  for (uint32_t i = 0; i < id_count; ++i) {
    if (NPN_IdentifierIsString(ids[i]) == named) {
      v8_array->Set(Integer::New(j), NPToV8Identifier(ids[i]));
      ++j;
    }
  }
  return v8_array;
}

Local<Array> NPV8Bridge::Enumerate(const NPObjectPtr<NPObject> np_object,
                                   bool named) {
  Local<Array> v8_array;

  // First try calling NPN_Enumerate. This will return false if the browser
  // does not support NPN_Enumerate.
  NPIdentifier* ids;
  uint32_t id_count;
  if (NPN_Enumerate(npp_, np_object.Get(), &ids, &id_count)) {
    v8_array = NPToV8IdentifierArray(ids, id_count, named);
    NPN_MemFree(ids);
  } else if (np_object->_class->structVersion >= NP_CLASS_STRUCT_VERSION_ENUM &&
             np_object->_class->enumerate != NULL &&
             np_object->_class->enumerate(np_object.Get(), &ids, &id_count)) {
    // Next see if the object has an enumerate callback and invoke it
    // directly.  This is the path used when V8 enumerates the
    // properties of a native object if the browser does not support
    // NPN_Enumerate.
    v8_array = NPToV8IdentifierArray(ids, id_count, named);
    NPN_MemFree(ids);
  } else {
    // The final fallback is to invoke a JavaScript function that
    // enumerates all the properties into an array and returns it to
    // the plugin.
    NPVariant np_result;
    NPVariant np_arg;
    OBJECT_TO_NPVARIANT(np_object.Get(), np_arg);
    if (NPN_InvokeDefault(npp_, np_enumerate_function_.Get(), &np_arg, 1,
                          &np_result)) {
      v8_array = NPToV8IdentifierArray(np_result, named);
      NPN_ReleaseVariantValue(&np_result);
    }
  }

  return v8_array;
}

v8::Handle<Value> NPV8Bridge::V8PropertyGetter(Local<Value> v8_name,
                                               const AccessorInfo& info) {
  Local<Value> v8_result;

  Local<Object> holder = info.Holder();
  NPV8Bridge* bridge = static_cast<NPV8Bridge*>(
      Local<External>::Cast(
          holder->GetInternalField(V8_NP_OBJECT_BRIDGE))->Value());
  Context::Scope scope(bridge->script_context());

  if (holder.IsEmpty())
    return v8_result;

  NPObjectPtr<NPObject> np_object = bridge->V8ToNPObject(holder);
  if (np_object.IsNull())
    return v8_result;

  NPIdentifier np_name = V8ToNPIdentifier(v8_name);
  if (np_name == NULL)
    return v8_result;

  NPVariant np_result;
  if (NPN_HasProperty(bridge->npp_, np_object.Get(), np_name) &&
      NPN_GetProperty(bridge->npp_, np_object.Get(), np_name, &np_result)) {
    v8_result = bridge->NPToV8Variant(np_result);
    NPN_ReleaseVariantValue(&np_result);
  } else if (np_object->_class->hasMethod != NULL &&
             np_object->_class->hasMethod(np_object.Get(), np_name)) {
    // It's not calling NPN_HasMethod here because of a bug in Firefox
    // (Mozilla bug ID 467945), where NPN_HasMethod forwards to the object's
    // hasProperty function instead. The workaround is to sidestep npruntime.
    v8_result = bridge->function_map_->Get(v8_name);
    if (v8_result.IsEmpty() || v8_result->IsUndefined()) {
      Local<FunctionTemplate> function_template =
          FunctionTemplate::New(V8CallNamedMethod, v8_name);
      v8_result = function_template->GetFunction();
      bridge->function_map_->Set(v8_name, v8_result);
    }
  }

  return v8_result;
}

v8::Handle<Value> NPV8Bridge::V8PropertySetter(
    Local<Value> v8_name,
    Local<Value> v8_value,
    const AccessorInfo& info) {
  Local<Value> v8_result;

  Local<Object> holder = info.Holder();
  NPV8Bridge* bridge = static_cast<NPV8Bridge*>(
      Local<External>::Cast(
          holder->GetInternalField(V8_NP_OBJECT_BRIDGE))->Value());
  Context::Scope scope(bridge->script_context());

  NPObjectPtr<NPObject> np_object = bridge->V8ToNPObject(holder);
  if (np_object.IsNull())
    return v8_result;

  NPIdentifier np_name = V8ToNPIdentifier(v8_name);
  if (np_name == NULL)
    return v8_result;

  NPVariant np_value = bridge->V8ToNPVariant(v8_value);
  NPN_SetProperty(bridge->npp_, np_object.Get(), np_name, &np_value);
  NPN_ReleaseVariantValue(&np_value);

  return v8_result;
}

v8::Handle<v8::Boolean> NPV8Bridge::V8PropertyQuery(Local<Value> v8_name,
                                                    const AccessorInfo& info) {
  Local<Object> holder = info.Holder();
  NPV8Bridge* bridge = static_cast<NPV8Bridge*>(
      Local<External>::Cast(
          holder->GetInternalField(V8_NP_OBJECT_BRIDGE))->Value());
  Context::Scope scope(bridge->script_context());

  NPObjectPtr<NPObject> np_object = bridge->V8ToNPObject(holder);
  if (np_object.IsNull())
    return v8::Handle<v8::Boolean>();

  NPIdentifier np_name = V8ToNPIdentifier(v8_name);
  if (np_name == NULL)
    return v8::Handle<v8::Boolean>();

  bool has = NPN_HasProperty(bridge->npp_, np_object.Get(), np_name) ||
             NPN_HasMethod(bridge->npp_, np_object.Get(), np_name);
  return v8::Boolean::New(has);
}

v8::Handle<v8::Boolean> NPV8Bridge::V8PropertyDeleter(
    Local<Value> v8_name,
    const AccessorInfo& info) {
  Local<Object> holder = info.Holder();
  NPV8Bridge* bridge = static_cast<NPV8Bridge*>(
      Local<External>::Cast(
          holder->GetInternalField(V8_NP_OBJECT_BRIDGE))->Value());
  Context::Scope scope(bridge->script_context());

  NPObjectPtr<NPObject> np_object = bridge->V8ToNPObject(holder);
  if (np_object.IsNull())
    return v8::Handle<v8::Boolean>();

  NPIdentifier np_name = V8ToNPIdentifier(v8_name);
  if (np_name == NULL)
    return v8::Handle<v8::Boolean>();

  // Workaround for a bug in Chrome. Chrome does not check whether the
  // removeProperty callback is implemented before calling it, causing
  // NPN_RemoveProperty to crash if it is not. So do the check before calling
  // it.
  bool deleted = np_object->_class->removeProperty != NULL &&
                 NPN_RemoveProperty(bridge->npp_, np_object.Get(), np_name);
  return v8::Boolean::New(deleted);
}

v8::Handle<Value> NPV8Bridge::V8NamedPropertyGetter(Local<v8::String> v8_name,
                                                    const AccessorInfo& info) {
  return V8PropertyGetter(v8_name, info);
}

v8::Handle<Value> NPV8Bridge::V8NamedPropertySetter(Local<v8::String> v8_name,
                                                    Local<Value> v8_value,
                                                    const AccessorInfo& info) {
  return V8PropertySetter(v8_name, v8_value, info);
}

v8::Handle<v8::Boolean> NPV8Bridge::V8NamedPropertyQuery(
    Local<v8::String> v8_name,
    const AccessorInfo& info) {
  return V8PropertyQuery(v8_name, info);
}

v8::Handle<v8::Boolean> NPV8Bridge::V8NamedPropertyDeleter(
    Local<v8::String> v8_name,
    const AccessorInfo& info) {
  return V8PropertyDeleter(v8_name, info);
}

v8::Handle<Array> NPV8Bridge::V8NamedPropertyEnumerator(
    const AccessorInfo& info) {
  Local<Object> holder = info.Holder();
  NPV8Bridge* bridge = static_cast<NPV8Bridge*>(
      Local<External>::Cast(
          holder->GetInternalField(V8_NP_OBJECT_BRIDGE))->Value());
  Context::Scope scope(bridge->script_context());

  NPObjectPtr<NPObject> np_object = bridge->V8ToNPObject(holder);
  if (np_object.IsNull())
    return v8::Handle<Array>();

  return bridge->Enumerate(np_object, true);
}

v8::Handle<Value> NPV8Bridge::V8IndexedPropertyGetter(
    uint32_t index,
    const AccessorInfo& info) {
  return V8PropertyGetter(Integer::New(index), info);
}

v8::Handle<Value> NPV8Bridge::V8IndexedPropertySetter(
    uint32_t index,
    Local<Value> v8_value,
    const AccessorInfo& info) {
  return V8PropertySetter(Integer::New(index), v8_value, info);
}

v8::Handle<v8::Boolean> NPV8Bridge::V8IndexedPropertyQuery(
    uint32_t index,
    const AccessorInfo& info) {
  return V8PropertyQuery(Integer::New(index), info);
}

v8::Handle<v8::Boolean> NPV8Bridge::V8IndexedPropertyDeleter(
    uint32_t index,
    const AccessorInfo& info) {
  return V8PropertyDeleter(Integer::New(index), info);
}

v8::Handle<Array> NPV8Bridge::V8IndexedPropertyEnumerator(
    const AccessorInfo& info) {
  Local<Object> holder = info.Holder();
  NPV8Bridge* bridge = static_cast<NPV8Bridge*>(
      Local<External>::Cast(
          holder->GetInternalField(V8_NP_OBJECT_BRIDGE))->Value());
  Context::Scope scope(bridge->script_context());

  NPObjectPtr<NPObject> np_object = bridge->V8ToNPObject(holder);
  if (np_object.IsNull())
    return v8::Handle<Array>();

  return bridge->Enumerate(np_object, false);
}

v8::Handle<Value> NPV8Bridge::V8CallNamedMethod(const Arguments& args) {
  Local<Value> v8_result;

  if (args.IsConstructCall())
    return v8_result;

  Local<Object> v8_holder = args.Holder();
  NPV8Bridge* bridge = static_cast<NPV8Bridge*>(
      Local<External>::Cast(
          v8_holder->GetInternalField(V8_NP_OBJECT_BRIDGE))->Value());
  Context::Scope scope(bridge->script_context());

  NPObjectPtr<NPObject> np_this = bridge->V8ToNPObject(v8_holder);
  if (np_this.IsNull())
    return v8_result;

  v8::Handle<Value> v8_name = args.Data();
  NPIdentifier np_name = V8ToNPIdentifier(v8_name);
  if (np_name == NULL)
    return v8_result;

  std::vector<NPVariant> np_args(args.Length());
  for (int i = 0; i != args.Length(); ++i) {
    np_args[i] = bridge->V8ToNPVariant(args[i]);
  }

  NPVariant np_result;
  if (NPN_Invoke(bridge->npp_,
                 np_this.Get(),
                 np_name,
                 args.Length() == 0 ? NULL : &np_args.front(),
                 args.Length(),
                 &np_result)) {
    v8_result = bridge->NPToV8Variant(np_result);
    NPN_ReleaseVariantValue(&np_result);
  }

  for (int i = 0; i != args.Length(); ++i) {
    NPN_ReleaseVariantValue(&np_args[i]);
  }

  return v8_result;
}

v8::Handle<Value> NPV8Bridge::V8CallFunction(const Arguments& args) {
  Local<Value> v8_result;

  NPV8Bridge* bridge = static_cast<NPV8Bridge*>(
      Local<External>::Cast(args.Data())->Value());
  Context::Scope scope(bridge->script_context());

  Local<Function> v8_callee = args.Callee();
  Local<Object> v8_this = args.This();

  // Allocate an extra argument element for the "this" pointer. This is only
  // used if we end up invoking a method through function.call(this, arg0, ...,
  // argn).
  std::vector<NPVariant> np_args(args.Length() + 1);
  VOID_TO_NPVARIANT(np_args[0]);
  for (int i = 0; i != args.Length(); ++i) {
    np_args[i + 1] = bridge->V8ToNPVariant(args[i]);
  }

  // Need to determine whether the object was called as a standalone function,
  // a method or a constructor. The constructor case is easy: args has a flag
  // for it. If the function was called standalone then "this" will reference
  // the global object. Otherwise assume it is a method invocation.
  NPVariant np_result;
  if (args.IsConstructCall()) {
    // NPN_Construct was giving me trouble on some browsers (like Chrome). It
    // might have better support in the future. For the time being, I'm using
    // this alternative.
    NPObjectPtr<NPObject> np_construct_function =
        bridge->GetNPConstructFunction(args.Length());
    np_args[0] = bridge->V8ToNPVariant(v8_callee);
    if (NPN_InvokeDefault(bridge->npp_, np_construct_function.Get(),
                          &np_args[0], args.Length() + 1, &np_result)) {
      v8_result = bridge->NPToV8Variant(np_result);
      NPN_ReleaseVariantValue(&np_result);
    }
  } else if (v8_this == bridge->script_context_->Global()) {
    // Treat standalone case specially. We use NPN_InvokeDefault rather than
    // NPN_Invoke with the "call" method because we want to have "this" refer
    // to the browser's global environment rather than the V8 global
    // environment.
    NPObjectPtr<NPObject> np_callee = bridge->V8ToNPObject(v8_callee);
    if (NPN_InvokeDefault(bridge->npp_, np_callee.Get(), 1 + &np_args[0],
                          args.Length(), &np_result)) {
      v8_result = bridge->NPToV8Variant(np_result);
      NPN_ReleaseVariantValue(&np_result);
    }
  } else {
    // Invoke a function as a method by invoking its "call" call method. This
    // is not the usual way of invoking a method in runtime. The usual way would
    // to be to call NPN_Invoke on the target object (the one to be bound to
    // "this") with a method name. But we don't know the method name. We don't
    // even know if the function is assigned to one of the properties of the
    // target object. To avoid that trouble, we invoke the function's "call"
    // method with "this" as an explicit argument.
    NPObjectPtr<NPObject> np_callee = bridge->V8ToNPObject(v8_callee);
    np_args[0] = bridge->V8ToNPVariant(v8_this);
    if (NPN_Invoke(bridge->npp_, np_callee.Get(), bridge->np_call_identifier_,
                   &np_args[0], args.Length() + 1, &np_result)) {
      v8_result = bridge->NPToV8Variant(np_result);
      NPN_ReleaseVariantValue(&np_result);
    }
  }

  for (int i = 0; i != args.Length() + 1; ++i) {
    NPN_ReleaseVariantValue(&np_args[i]);
  }

  return v8_result;
}

v8::Handle<Value> NPV8Bridge::V8CallAsFunction(const Arguments& args) {
  Local<Value> v8_result;

  Local<Object> v8_callee = args.This();
  NPV8Bridge* bridge = static_cast<NPV8Bridge*>(
      Local<External>::Cast(
          v8_callee->GetInternalField(V8_NP_OBJECT_BRIDGE))->Value());
  Context::Scope scope(bridge->script_context());

  std::vector<NPVariant> np_args(args.Length());
  for (int i = 0; i != args.Length(); ++i) {
    np_args[i] = bridge->V8ToNPVariant(args[i]);
  }

  NPVariant np_result;
  NPObjectPtr<NPObject> np_callee = bridge->V8ToNPObject(v8_callee);
  if (NPN_InvokeDefault(bridge->npp_, np_callee.Get(),
                        args.Length() == 0 ? NULL : &np_args[0], args.Length(),
                        &np_result)) {
    v8_result = bridge->NPToV8Variant(np_result);
    NPN_ReleaseVariantValue(&np_result);
  }

  for (int i = 0; i != args.Length(); ++i) {
    NPN_ReleaseVariantValue(&np_args[i]);
  }

  return v8_result;
}

// Evaluates and returns an NP function that will construct an object. The
// function takes the constructor and constructor arguments as arguments.
// I'm doing this because not all browsers seem to support calling NPN_Construct
// on JavaScript constructor functions.
NPObjectPtr<NPObject> NPV8Bridge::GetNPConstructFunction(int arity) {
  NPConstructFunctionMap::const_iterator it = np_construct_functions_.find(
      arity);
  if (it != np_construct_functions_.end())
    return it->second;

  // Build a function that looks like:
  // (function (c,p0,p1) { return new c(p0,p1); })
  std::ostringstream code;
  code << "(function(c";
  for (int i = 0; i != arity; ++i) {
    code << ",p" << i;
  }
  code << ") { return new c(";
  String separator = "";
  for (int i = 0; i != arity; ++i) {
    code << separator << String("p") << i;
    separator = ",";
  }
  code << "); })";

  return NPEvaluateObject(code.str().c_str());
}
}  // namespace o3d
