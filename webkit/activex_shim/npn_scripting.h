// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_ACTIVEX_SHIM_NPN_SCRIPTING_H__
#define WEBKIT_ACTIVEX_SHIM_NPN_SCRIPTING_H__

#include <string>
#include "webkit/glue/plugins/nphostapi.h"

namespace activex_shim {

class NPNWindow;
class NPNDocument;

// Simplifies querying and calling methods of a scriptable NPObject
class NPNScriptableObject {
 public:
  NPNScriptableObject();
  // The caller should leave the NPObject to me and don't worry about releasing
  // the NPObject.
  NPNScriptableObject(NPP npp, NPObject* object);
  // Implement copy constructor so that we could easily do things like:
  //   NPNScriptableObject location = window.GetObjectProperty("location");
  // In this copy constructor we will ask the browser to add ref to the
  // underlying NPObject (retainobject).
  NPNScriptableObject(NPNScriptableObject& object);
  // We will call the browser to release the object.
  ~NPNScriptableObject();

  // We will take over the object. Thus we don't add ref here.
  void Init(NPP npp, NPObject* object);
  // Release container object and set it to NULL.
  void Release();
  // Reference counting for assignment
  NPNScriptableObject& operator=(NPNScriptableObject& object);

  bool IsValid() { return object_ != NULL; }
  bool HasProperty(const std::string& name);
  // Caller is responsible to release the ret value if succeeded.
  bool GetProperty(const std::string& name, NPVariant* ret);
  bool SetProperty(const std::string& name, const NPVariant& val);

  // More specific Get/Set properties for convenience.
  // If you don't care if the call was successful, you can pass succeeded as
  // NULL. It is used to differentiate the case when we get an empty string,
  // and don't know if it is because the call failed or not.
  std::wstring GetStringProperty(const std::string& name, bool* succeeded);
  std::wstring GetStringProperty(const std::string& name) {
    return GetStringProperty(name, NULL);
  }
  bool SetStringProperty(const std::string& name, const std::wstring& val);
  // See comments for GetStringProperty.
  NPNScriptableObject GetObjectProperty(const std::string& name,
                                        bool* succeeded);
  NPNScriptableObject GetObjectProperty(const std::string& name) {
    return GetObjectProperty(name, NULL);
  }

  // Invoke a method of the NPObject. format should be exactly like (only the
  // ones listed here are supported):
  //   %s: the following param is a null terminated wchar_t* string.
  //   %d: the following param is an integer.
  // Example: window.Invoke("open", "%s%s", L"http://b", L"_blank")
  // This is only used internally so we have control over format string.
  bool Invoke(const std::string& name, const char* format, ...);

 private:
  // Which NP instance created me. Used to pass as parameters in NPN_Invoke
  // like calls.
  NPP npp_;
  // The NPObject that I am operating on.
  NPObject* object_;
};

// Helper class to simplify use of NPVariant. Never add any virtual function
// or member variables to this class because we will use it like
// vector<NPVariantWrap> and pass the internal array as NPVariant* to NPAPI
// calls.
struct NPVariantWrap : public NPVariant {
  NPVariantWrap();
  NPVariantWrap(const NPVariantWrap& v);
  ~NPVariantWrap();
  NPVariantWrap& operator=(const NPVariantWrap& v);
  void Release();
  void Copy(const NPVariant& v);
  void SetBool(bool val);
  void SetInt(int val);
  void SetString(const std::wstring& val);
  void SetUTF8String(const NPUTF8* p, unsigned int size);
};

}  // namespace activex_shim

#endif // #ifndef WEBKIT_ACTIVEX_SHIM_NPN_SCRIPTING_H__
