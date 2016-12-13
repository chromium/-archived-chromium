// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/activex_shim/npn_scripting.h"

#include "base/logging.h"
#include "base/string_util.h"
#include "webkit/activex_shim/npp_impl.h"

namespace activex_shim {

NPNScriptableObject::NPNScriptableObject() : npp_(NULL), object_(NULL) {
}

NPNScriptableObject::NPNScriptableObject(NPP npp, NPObject* object)
    : npp_(npp),
      object_(object) {
}

NPNScriptableObject::NPNScriptableObject(NPNScriptableObject& object)
    : npp_(object.npp_),
      object_(object.object_) {
  if (object_)
    g_browser->retainobject(object_);
}

NPNScriptableObject::~NPNScriptableObject() {
  Release();
}

NPNScriptableObject& NPNScriptableObject::operator=(
    NPNScriptableObject& object) {
  if (this == &object)
    return *this;
  if (object_)
    g_browser->releaseobject(object_);
  npp_ = object.npp_;
  object_ = object.object_;
  if (object_)
    g_browser->retainobject(object_);
  return *this;
}

void NPNScriptableObject::Init(NPP npp, NPObject* object) {
  Release();
  npp_ = npp;
  object_ = object;
}

void NPNScriptableObject::Release() {
  if (object_) {
    g_browser->releaseobject(object_);
    object_ = NULL;
  }
  npp_ = NULL;
}

bool NPNScriptableObject::HasProperty(const std::string& name) {
  if (!IsValid())
    return false;
  NPIdentifier id = g_browser->getstringidentifier(name.c_str());
  bool res = g_browser->hasproperty(npp_, object_, id);
  // How do we release id since it's coming from the browser? the answer is
  // we never release them. See implementation of NPN_GetStringIdentifier
  // in npruntime.cpp.
  return res;
}

bool NPNScriptableObject::GetProperty(const std::string& name, NPVariant* ret) {
  if (!IsValid())
    return false;
  NPIdentifier id = g_browser->getstringidentifier(name.c_str());
  return g_browser->getproperty(npp_, object_, id, ret);
}

bool NPNScriptableObject::SetProperty(const std::string& name,
                                      const NPVariant& val) {
  if (!IsValid())
    return false;
  NPIdentifier id = g_browser->getstringidentifier(name.c_str());
  return g_browser->setproperty(npp_, object_, id, &val);
}

NPNScriptableObject NPNScriptableObject::GetObjectProperty(
    const std::string& name,
    bool* succeeded) {
  NPNScriptableObject res;
  if (succeeded)
    *succeeded = false;
  NPVariant var;
  if (!GetProperty(name, &var))
    return res;
  if (var.type == NPVariantType_Object) {
    res.Init(npp_, var.value.objectValue);
    // From now, the object should have reference count as 1. We shall not
    // release the variant cause it will release the object.
    if (succeeded)
      *succeeded = true;
  } else {
    g_browser->releasevariantvalue(&var);
  }
  return res;
}

std::wstring NPNScriptableObject::GetStringProperty(const std::string& name,
                                                    bool* succeeded) {
  std::wstring res;
  if (succeeded)
    *succeeded = false;
  NPVariant var;
  if (!GetProperty(name, &var))
    return res;
  if (var.type == NPVariantType_String) {
    std::string tmp(var.value.stringValue.UTF8Characters,
                    var.value.stringValue.UTF8Length);
    res = UTF8ToWide(tmp.c_str());
    if (succeeded)
      *succeeded = true;
  }
  // We've made a copy of the string. Thus we should release the variant in
  // any case.
  g_browser->releasevariantvalue(&var);
  return res;
}

bool NPNScriptableObject::SetStringProperty(const std::string& name,
                                            const std::wstring& val) {
  NPVariantWrap var;
  var.SetString(val);
  return SetProperty(name, var);
}

bool NPNScriptableObject::Invoke(const std::string& name,
                                 const char* format, ...) {
  if (!IsValid())
    return false;
  NPIdentifier id = g_browser->getstringidentifier(name.c_str());
  std::vector<NPVariantWrap> args;
  va_list argptr;
  va_start(argptr, format);
  for (const char* p = format; *p; ++p) {
    char c = *p;
    DCHECK(c == '%' && *(p + 1) != 0);
    if (c != '%' || *(p + 1) == 0)
      continue;
    ++p;
    c = *p;
    NPVariantWrap var;
    switch(c) {
      case 's': {
        // String
        wchar_t *s = va_arg(argptr, wchar_t*);
        var.SetString(s);
        break;
      }
      case 'd': {
        // String
        int n = va_arg(argptr, int);
        var.SetInt(n);
        break;
      }
      default: {
        DCHECK(false);
        break;
      }
    }
    args.push_back(var);
  }
  NPVariant ret;
  ret.type = NPVariantType_Void;
  bool res = false;
  if (args.size())
    res = g_browser->invoke(npp_, object_, id, &args[0],
                            static_cast<unsigned int>(args.size()), &ret);
  else
    res = g_browser->invoke(npp_, object_, id, NULL, 0, &ret);

  g_browser->releasevariantvalue(&ret);
  return res;
}

NPVariantWrap::NPVariantWrap() {
  type = NPVariantType_Void;
}

NPVariantWrap::NPVariantWrap(const NPVariantWrap& v) {
  type = NPVariantType_Void;
  Copy(v);
}

NPVariantWrap::~NPVariantWrap() {
  Release();
}

NPVariantWrap& NPVariantWrap::operator=(const NPVariantWrap& v) {
  if (this != &v)
    Copy(v);
  return *this;
}

void NPVariantWrap::Copy(const NPVariant& v) {
  if (this == &v)
    return;
  Release();
  switch(v.type) {
    case NPVariantType_Void:
      break;
    case NPVariantType_Null:
      break;
    case NPVariantType_Bool:
      value.boolValue = v.value.boolValue;
      break;
    case NPVariantType_Int32:
      value.intValue = v.value.intValue;
      break;
    case NPVariantType_Double:
      value.doubleValue = v.value.doubleValue;
      break;
    case NPVariantType_String:
      SetUTF8String(v.value.stringValue.UTF8Characters,
                    v.value.stringValue.UTF8Length);
      break;
    case NPVariantType_Object:
      g_browser->retainobject(v.value.objectValue);
      value.objectValue = v.value.objectValue;
      break;
    default:
      DCHECK(false);
      break;
  }
  type = v.type;
}

void NPVariantWrap::Release() {
  if (type == NPVariantType_String) {
    delete[] value.stringValue.UTF8Characters;
  } else if (type == NPVariantType_Object) {
    g_browser->releaseobject(value.objectValue);
  }
  type = NPVariantType_Void;
}

void NPVariantWrap::SetBool(bool val) {
  Release();
  value.boolValue = val;
  type = NPVariantType_Bool;
}

void NPVariantWrap::SetInt(int val) {
  Release();
  value.intValue = val;
  type = NPVariantType_Int32;
}

void NPVariantWrap::SetUTF8String(const NPUTF8* p, unsigned int size) {
  Release();
  value.stringValue.UTF8Characters = new char[size + 1];
  memcpy(const_cast<NPUTF8*>(value.stringValue.UTF8Characters),
         p, size + 1);
  value.stringValue.UTF8Length = size;
  type = NPVariantType_String;
}

void NPVariantWrap::SetString(const std::wstring& val) {
  std::string s = WideToUTF8(val);
  SetUTF8String(s.c_str(), static_cast<unsigned int>(s.size()));
}

}  // namespace activex_shim
