// Copyright 2009, Google Inc.  All rights reserved.
// Portions of this file were adapted from the Mozilla project.
// See https://developer.mozilla.org/en/ActiveX_Control_for_Hosting_Netscape_Plug-ins_in_IE
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the Netscape security libraries.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1994-2000
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Adam Lock <adamlock@eircom.net>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include <atlstr.h>

#include <string>
#include <set>

#include "plugin/npapi_host_control/win/host_control.h"
#include "plugin/npapi_host_control/win/variant_utils.h"

namespace {

// Helper routine that invokes the host-control stream request function.
NPError OpenURL(NPBrowserProxy* browser_proxy,
                const char *szURL,
                const char *szTarget,
                void *pNotifyData) {
  CHostControl* host_control = browser_proxy->GetHostingControl();

  USES_CONVERSION;
  HRESULT hr = host_control->OpenUrlStream(A2CW(szURL), pNotifyData);
  return SUCCEEDED(hr) ? NPERR_NO_ERROR : NPERR_GENERIC_ERROR;
}
}  // unnamed namespace

NPNetscapeFuncs NPBrowserProxy::kNetscapeFunctions = {
  sizeof(kNetscapeFunctions),
  NP_VERSION_MAJOR << 8 | NP_VERSION_MINOR,
  NPN_GetURL,
  NPN_PostURL,
  NPN_RequestRead,
  NPN_NewStream,
  NPN_Write,
  NPN_DestroyStream,
  NPN_Status,
  NPN_UserAgent,
  NPN_MemAlloc,
  NPN_MemFree,
  NPN_MemFlush,
  NPN_ReloadPlugins,
  NPN_GetJavaEnv,
  NPN_GetJavaPeer,
  NPN_GetURLNotify,
  NPN_PostURLNotify,
  NPN_GetValue,
  NPN_SetValue,
  NPN_InvalidateRect,
  NPN_InvalidateRegion,
  NPN_ForceRedraw,
  NPN_GetStringIdentifier,
  NPN_GetStringIdentifiers,
  NPN_GetIntIdentifier,
  NPN_IdentifierIsString,
  NPN_UTF8FromIdentifier,
  NPN_IntFromIdentifier,
  NPN_CreateObject,
  NPN_RetainObject,
  NPN_ReleaseObject,
  NPN_Invoke,
  NPN_InvokeDefault,
  NPN_Evaluate,
  NPN_GetProperty,
  NPN_SetProperty,
  NPN_RemoveProperty,
  NPN_HasProperty,
  NPN_HasMethod,
  NPN_ReleaseVariantValue,
  NPN_SetException,
  NULL,
  NULL,
  NPN_Enumerate,
};

NPBrowserProxy::NPBrowserProxy(CHostControl* host, IDispatchEx* window_dispatch)
    : host_control_(host) {
  vwindow_object_ = new DispatchProxy(window_dispatch, this);

  CComPtr<IUnknown> unknown_identity;
  HRESULT hr = window_dispatch->QueryInterface(&unknown_identity);
  ATLASSERT(SUCCEEDED(hr));

  dispatch_proxy_map_[unknown_identity] = vwindow_object_;
  call_identifier_ = NPN_GetStringIdentifier("call");
}

NPBrowserProxy::~NPBrowserProxy() {
  for (NPObjectProxyMap::iterator it = np_object_proxy_map_.begin();
       it != np_object_proxy_map_.end(); ++it) {
    it->second->SetBrowserProxy(NULL);
  }
  for (DispatchProxyMap::iterator it = dispatch_proxy_map_.begin();
       it != dispatch_proxy_map_.end(); ++it) {
    it->second->SetBrowserProxy(NULL);
    GetBrowserFunctions()->releaseobject(it->second);
  }
}

CComPtr<IDispatchEx> NPBrowserProxy::GetDispatchObject(NPObject* np_object) {
  if (np_object == NULL) {
    return CComPtr<IDispatchEx>();
  }

  // If the NPObject is already wrapping an IDispatchEx interface, then
  // return that interface directly.
  if (np_object->_class == DispatchProxy::GetNPClass()) {
    DispatchProxy *dispatch_proxy =
        static_cast<DispatchProxy*>(np_object);
    return dispatch_proxy->GetDispatchEx();
  } else {
    // If the NPObject already has a proxy then return that.
    NPObjectProxyMap::iterator it = np_object_proxy_map_.find(np_object);
    if (it != np_object_proxy_map_.end()) {
      CComPtr<IDispatchEx> dispatch_ex(NULL);
      HRESULT hr = it->second.QueryInterface(&dispatch_ex);
      if (SUCCEEDED(hr)) {
        return dispatch_ex;
      } else {
        return NULL;
      }
    }

    // Create a new NPObject proxy, register it for future use and return it.
    CComPtr<INPObjectProxy> proxy_wrapper;
    HRESULT hr = NPObjectProxy::CreateInstance(&proxy_wrapper);
    if (SUCCEEDED(hr)) {
      proxy_wrapper->SetBrowserProxy(this);
      proxy_wrapper->SetHostedObject(np_object);
      RegisterNPObjectProxy(np_object, proxy_wrapper);

      CComPtr<IDispatchEx> dispatch_proxy_wrapper;
      hr = proxy_wrapper.QueryInterface(&dispatch_proxy_wrapper);
      ATLASSERT(SUCCEEDED(hr));
      return dispatch_proxy_wrapper;
    }
  }
  return CComPtr<IDispatchEx>();
}

NPObject* NPBrowserProxy::GetNPObject(IDispatch* dispatch_object) {
  if (dispatch_object == NULL)
    return NULL;

  // If the COM object is already wrapping an NPObject then return that NPObject
  // directly.
  NPObject* np_object;
  CComPtr<INPObjectProxy> np_object_proxy;
  if (SUCCEEDED(dispatch_object->QueryInterface(&np_object_proxy))) {
    if (SUCCEEDED(np_object_proxy->GetNPObjectInstance(
        reinterpret_cast<void**>(&np_object))))
      return np_object;
    else
      return NULL;
  } else {
    CComPtr<IUnknown> unknown_identity;
    if (FAILED(dispatch_object->QueryInterface(&unknown_identity))) {
      return NULL;
    }

    // If the COM object already has a proxy then return that.  Note that the
    // map is keyed on IUnknown ptrs - this is because COM explicitly states
    // that the IUnknown interface is the only reliable identity mechanism.
    DispatchProxyMap::iterator it = dispatch_proxy_map_.find(unknown_identity);
    if (it != dispatch_proxy_map_.end()) {
      GetBrowserFunctions()->retainobject(it->second);
      return it->second;
    }

    // Create a new DispatchProxy.
    CComPtr<IDispatchEx> dispatchex_object;
    if (FAILED(dispatch_object->QueryInterface(&dispatchex_object))) {
      return NULL;
    }
    DispatchProxy* dispatch_proxy = new DispatchProxy(dispatchex_object,
                                                      this);
    dispatch_proxy_map_[unknown_identity] = dispatch_proxy;
    return dispatch_proxy;
  }
}

void NPBrowserProxy::RegisterNPObjectProxy(
    NPObject* np_object,
    const CComPtr<INPObjectProxy>& proxy_wrapper) {
  np_object_proxy_map_[np_object] = proxy_wrapper;
}

void NPBrowserProxy::UnregisterNPObjectProxy(NPObject* np_object) {
  np_object_proxy_map_.erase(np_object);
}

void NPBrowserProxy::UnregisterDispatchProxy(IDispatchEx* dispatch_object) {
  CComPtr<IUnknown> unknown_identity;
  HRESULT hr = dispatch_object->QueryInterface(&unknown_identity);
  ATLASSERT(SUCCEEDED(hr));
  if (!SUCCEEDED(hr)) {
    return;
  }

  DispatchProxyMap::iterator it = dispatch_proxy_map_.find(unknown_identity);
  ATLASSERT(it != dispatch_proxy_map_.end());
  GetBrowserFunctions()->releaseobject(it->second);
  dispatch_proxy_map_.erase(it);
}

NPError NPBrowserProxy::NPN_GetURL(NPP npp,
                                   const char* relativeURL,
                                   const char* target) {
  if (!npp) {
    return NPERR_INVALID_INSTANCE_ERROR;
  }
  ATLASSERT(false && "NPN_GetURL not implemented");
  return NPERR_NO_ERROR;
}


NPError NPBrowserProxy::NPN_GetURLNotify(NPP npp,
                                         const char* relativeURL,
                                         const char* target,
                                         void* notifyData) {
  if (!npp) {
    return NPERR_INVALID_INSTANCE_ERROR;
  }

  NPBrowserProxy *browser_proxy = static_cast<NPBrowserProxy*>(npp->ndata);
  return OpenURL(browser_proxy, relativeURL, target, notifyData);
}



NPError NPBrowserProxy::NPN_PostURLNotify(NPP npp,
                                          const char *relativeURL,
                                          const char *target,
                                          uint32 len,
                                          const char *buf,
                                          NPBool file,
                                          void *notifyData) {
  if (!npp) {
    return NPERR_INVALID_INSTANCE_ERROR;
  }
  ATLASSERT(false && "NPN_PostURLNotify not implemented.");
  return NPERR_NO_ERROR;
}

NPError NPBrowserProxy::NPN_PostURL(NPP npp,
                                    const char *relativeURL,
                                    const char *target,
                                    uint32 len,
                                    const char *buf,
                                    NPBool file) {
  if (!npp) {
    return NPERR_INVALID_INSTANCE_ERROR;
  }
  ATLASSERT(false && "NPN_PostURL not implemented.");
  return NPERR_NO_ERROR;
}

NPError NPBrowserProxy::NPN_NewStream(NPP npp,
                                      NPMIMEType type,
                                      const char* window,
                                      NPStream* *result) {
  if (!npp) {
    return NPERR_INVALID_INSTANCE_ERROR;
  }
  ATLASSERT(false && "NPN_NewStream not implemented.");
  return NPERR_GENERIC_ERROR;
}


int32 NPBrowserProxy::NPN_Write(NPP npp,
                                NPStream *pstream,
                                int32 len,
                                void *buffer) {
  if (!npp) {
    return NPERR_INVALID_INSTANCE_ERROR;
  }
  ATLASSERT(false && "NPN_Write not implemented.");
  return NPERR_GENERIC_ERROR;
}

NPError NPBrowserProxy::NPN_DestroyStream(NPP npp,
                                          NPStream *pstream,
                                          NPError reason) {
  if (!npp) {
    return NPERR_INVALID_INSTANCE_ERROR;
  }
  ATLASSERT(false && "NPN_DestroyStream not implemented.");
  return NPERR_GENERIC_ERROR;
}

void NPBrowserProxy::NPN_Status(NPP npp, const char *message) {
  if (!npp) {
    return;
  }
}

void *NPBrowserProxy::NPN_MemAlloc(uint32 size) {
  return malloc(size);
}

void NPBrowserProxy::NPN_MemFree(void *ptr) {
  if (ptr) {
    free(ptr);
  }
}

uint32 NPBrowserProxy::NPN_MemFlush(uint32 size) {
  return 0;
}

void NPBrowserProxy::NPN_ReloadPlugins(NPBool reloadPages) {
  ATLASSERT(false && "NPN_ReloadPlugins not implemented.");
}

void NPBrowserProxy::NPN_InvalidateRect(NPP npp, NPRect *invalidRect) {
  if (!npp) {
    return;
  }
  ATLASSERT(false && "NPN_InvalidateRect not implemented.");
}

void NPBrowserProxy::NPN_InvalidateRegion(NPP npp, NPRegion invalidRegion) {
  if (!npp) {
    return;
  }
  ATLASSERT(false && "NPN_InvalidateRect not implemented.");
}

void NPBrowserProxy::NPN_ForceRedraw(NPP npp) {
  if (!npp) {
    return;
  }
  ATLASSERT(false && "NPN_ForceRedraw not implemented.");
}

NPError NPBrowserProxy::NPN_GetValue(NPP npp,
                                     NPNVariable variable,
                                     void *result) {
  if (!npp) {
    return NPERR_INVALID_INSTANCE_ERROR;
  }

  if (!result) {
    return NPERR_INVALID_PARAM;
  }

  NPBrowserProxy *browser_proxy = static_cast<NPBrowserProxy*>(npp->ndata);
  ATLASSERT(browser_proxy);

  switch (variable) {
    case NPNVxDisplay :
      return NPERR_GENERIC_ERROR;
    case NPNVnetscapeWindow:
      *(static_cast<HWND*>(result)) =
          browser_proxy->GetHostingControl()->m_hWnd;
      break;
    case NPNVjavascriptEnabledBool :
      *(static_cast<NPBool*>(result)) = TRUE;
      break;
    case NPNVasdEnabledBool :
      *(static_cast<NPBool*>(result)) = FALSE;
      break;
    case NPNVisOfflineBool :
      *(reinterpret_cast<NPBool*>(result)) = FALSE;
      break;
    case NPNVWindowNPObject :
      ++browser_proxy->GetVWindowObject()->referenceCount;
      *(static_cast<NPObject**>(result)) =
          browser_proxy->GetVWindowObject();
      break;
    case NPNVPluginElementNPObject :
      ATLASSERT(false && "NPNVPluginElementNPObject not supported.");
      return NPERR_GENERIC_ERROR;
    default:
      ATLASSERT(false && "Unrecognized NPN_GetValue request.");
      return NPERR_GENERIC_ERROR;
  }

  return NPERR_NO_ERROR;
}

NPError NPBrowserProxy::NPN_SetValue(NPP npp,
                                     NPPVariable variable,
                                     void *result) {
  if (!npp) {
    return NPERR_INVALID_INSTANCE_ERROR;
  }
  ATLASSERT(false && "NPN_SetValue not implemented.");
  return NPERR_GENERIC_ERROR;
}

NPError NPBrowserProxy::NPN_RequestRead(NPStream *pstream,
                                        NPByteRange *rangeList) {
  if (!pstream || !rangeList || !pstream->ndata) {
    return NPERR_INVALID_PARAM;
  }
  ATLASSERT(false && "NPN_RequestRead not implemented.");
  return NPERR_GENERIC_ERROR;
}

void* NPBrowserProxy::NPN_GetJavaEnv() {
  return NULL;
}


const char* NPBrowserProxy::NPN_UserAgent(NPP npp) {
  if (!npp) {
    return "";
  }

  NPBrowserProxy *browser_proxy = static_cast<NPBrowserProxy*>(npp->ndata);
  ATLASSERT(browser_proxy);
  return browser_proxy->GetHostingControl()->GetUserAgent();
}

void* NPBrowserProxy::NPN_GetJavaClass(void* handle) {
  return NULL;
}

void* NPBrowserProxy::NPN_GetJavaPeer(NPP npp) {
  return NULL;
}

NPObject* NPBrowserProxy::NPN_CreateObject(NPP npp,
                                           NPClass *aClass) {
  if (!npp || !aClass) {
    return NULL;
  }

  NPObject *new_object = NULL;

  // If the class exports a custom allocation routine, then invoke that.
  if (aClass->allocate) {
    new_object = aClass->allocate(npp, aClass);
    new_object->_class = aClass;
  } else {
    new_object = new NPObject;
    new_object->_class = aClass;
  }

  new_object->referenceCount = 1;

  return new_object;
}

NPObject * NPBrowserProxy::NPN_RetainObject(NPObject *obj) {
  if (obj) {
    ++obj->referenceCount;
  }
  return obj;
}


void NPBrowserProxy::NPN_ReleaseObject(NPObject *object) {
  if (object) {
    if (0 == --object->referenceCount) {
      if (object->_class->deallocate) {
        object->_class->deallocate(object);
      } else {
        delete object;
      }
    }
  }
}

// Disable warnings concerning reinterpret casts to un-related pointer types
// for the below functions.
#pragma warning(push)
#pragma warning(disable:4312)
#pragma warning(disable:4311)

namespace {
const uint32_t kNPIdentifierIntFlag = 0x1;
}

NPIdentifier NPBrowserProxy::NPN_GetStringIdentifier(const NPUTF8 *name) {
  // Note that this routine returns the address of the string as it is stored
  // in a set.  This implies that any virtual address value could be the
  // returned identifier.  The name & description properties of the CHostControl
  // object will not conflict with these, because they are within the VMA region
  // that is unmapped.
  static std::set<std::string> identifiers;
  std::string std_name(name);
  std::pair<std::set<std::string>::iterator, bool> result =
      identifiers.insert(std_name);
  const std::string& key = *(result.first);
  uint32_t tag = reinterpret_cast<uint32_t>(&key);
  ATLASSERT(0 == (tag & kNPIdentifierIntFlag));
  return reinterpret_cast<NPIdentifier>(tag);
}

void NPBrowserProxy::NPN_GetStringIdentifiers(const NPUTF8 **names,
                                              int32_t nameCount,
                                              NPIdentifier *identifiers) {
  for (int x = 0; x < nameCount; ++x) {
    identifiers[x] = NPN_GetStringIdentifier(names[x]);
  }
}

NPUTF8 * NPBrowserProxy::NPN_UTF8FromIdentifier(NPIdentifier identifier) {
  ATLASSERT(identifier != NULL);
  int32_t tag = reinterpret_cast<uint32_t>(identifier);
  if (0 == (tag & kNPIdentifierIntFlag)) {
    const std::string* key = reinterpret_cast<const std::string*>(tag);
    NPUTF8* identifier_value = static_cast<NPUTF8*>(
        NPN_MemAlloc(static_cast<uint32_t>(key->length() + 1)));
    memcpy(identifier_value, key->c_str(), key->length() + 1);
    return identifier_value;
  } else {
    // This is not a standard feature of NPN_UTF8FromIdentifier. Normally you
    // cannot convert an integer identifier to a string. We support it here
    // because IE and COM represent integer identifiers as strings in places.
    // For example, if IDispathEx::GetMemberName is invoked with an id for
    // an integer indexed property, the only things to do are return a
    // string representation of the integer or an error.
    CStringA string;
    string.Format("%d", tag >> 1);
    NPUTF8* identifier_value = static_cast<NPUTF8*>(
        NPN_MemAlloc(string.GetLength() + 1));
    memcpy(identifier_value, string.GetBuffer(), string.GetLength() + 1);
    return identifier_value;
  }
}

NPIdentifier NPBrowserProxy::NPN_GetIntIdentifier(int32_t intid) {
  ATLASSERT(intid <= 0x7FFFFFFF);
  return reinterpret_cast<NPIdentifier>((intid << 1) | kNPIdentifierIntFlag);
}

int32_t NPBrowserProxy::NPN_IntFromIdentifier(NPIdentifier identifier) {
  ATLASSERT(identifier != NULL);
  int32_t tag = reinterpret_cast<int32_t>(identifier);
  ATLASSERT(kNPIdentifierIntFlag == (tag & kNPIdentifierIntFlag));
  return tag >> 1;
}

bool NPBrowserProxy::NPN_IdentifierIsString(NPIdentifier identifier) {
  ATLASSERT(identifier != NULL);
  uint32_t tag = reinterpret_cast<uint32_t>(identifier);
  return (tag & kNPIdentifierIntFlag) == 0;
}

#pragma warning(pop)

void NPBrowserProxy::NPN_ReleaseVariantValue(NPVariant *variant) {
  switch (variant->type) {
    case NPVariantType_Void:
    case NPVariantType_Null:
    case NPVariantType_Bool:
    case NPVariantType_Int32:
    case NPVariantType_Double:
      break;
    case NPVariantType_String:
      NPN_MemFree(
          const_cast<NPUTF8*>(variant->value.stringValue.utf8characters));
      break;
    case NPVariantType_Object:
      NPN_ReleaseObject(variant->value.objectValue);
      break;
    default:
      ATLASSERT(false && "Unrecognized NPVariant type.");
      break;
  }
}

bool NPBrowserProxy::NPN_GetProperty(NPP npp,
                                     NPObject *obj,
                                     NPIdentifier propertyName,
                                     NPVariant *result) {
  if (!npp || !obj || !obj->_class->getProperty) {
    return false;
  }
  return obj->_class->getProperty(obj, propertyName, result);
}

bool NPBrowserProxy::NPN_SetProperty(NPP npp,
                                     NPObject *obj,
                                     NPIdentifier propertyName,
                                     const NPVariant *value) {
  if (!npp || !obj || !obj->_class->setProperty) {
    return false;
  }
  return obj->_class->setProperty(obj, propertyName, value);
}

bool NPBrowserProxy::NPN_HasProperty(NPP npp,
                                     NPObject *npobj,
                                     NPIdentifier propertyName) {
  if (!npp || !npobj || !npobj->_class->hasProperty) {
    return false;
  }
  return npobj->_class->hasProperty(npobj, propertyName);
}

bool NPBrowserProxy::NPN_RemoveProperty(NPP npp,
                                        NPObject *npobj,
                                        NPIdentifier propertyName) {
  if (!npp || !npobj || !npobj->_class->removeProperty) {
    return false;
  }
  return npobj->_class->removeProperty(npobj, propertyName);
}

bool NPBrowserProxy::NPN_HasMethod(NPP npp,
                                   NPObject *npobj,
                                   NPIdentifier methodName) {
  if (!npp || !npobj || !npobj->_class->hasMethod) {
    return false;
  }
  return npobj->_class->hasMethod(npobj, methodName);
}

bool NPBrowserProxy::NPN_Invoke(NPP npp,
                                NPObject *obj,
                                NPIdentifier methodName,
                                const NPVariant *args,
                                unsigned argCount,
                                NPVariant *result) {
  if (!npp || !obj || !obj->_class->invoke) {
    return false;
  }
  return obj->_class->invoke(obj, methodName, args, argCount, result);
}

bool NPBrowserProxy::NPN_InvokeDefault(NPP npp,
                                       NPObject *obj,
                                       const NPVariant *args,
                                       unsigned argCount,
                                       NPVariant *result) {
  if (!npp || !obj || !obj->_class->invokeDefault) {
    return false;
  }
  return obj->_class->invokeDefault(obj, args, argCount, result);
}

bool NPBrowserProxy::NPN_Construct(NPP npp,
                                   NPObject *obj,
                                   const NPVariant *args,
                                   unsigned argCount,
                                   NPVariant *result) {
  if (!npp || !obj || !obj->_class->construct) {
    return false;
  }
  return obj->_class->construct(obj, args, argCount, result);
}

bool NPBrowserProxy::NPN_Enumerate(NPP npp,
                                   NPObject* obj,
                                   NPIdentifier** ids,
                                   uint32_t* idCount) {
  if (!npp || !obj || !obj->_class->enumerate || !ids || !idCount) {
    return false;
  }
  return obj->_class->enumerate(obj, ids, idCount);
}

// Construct a new JavaScript object using the given global constructor
// and argument values.
bool NPBrowserProxy::ConstructObject(NPP npp,
                                     NPObject* window_object,
                                     NPUTF8* constructor_name,
                                     NPVariant* args,
                                     uint32_t numArgs,
                                     NPObject** result) {
  bool success = false;
  NPIdentifier constructor_identifier = NPN_GetStringIdentifier(
      constructor_name);
  NPVariant constructor_variant;
  if (NPN_GetProperty(npp, window_object, constructor_identifier,
                      &constructor_variant)) {
    if (NPVARIANT_IS_OBJECT(constructor_variant)) {
      NPObject* constructor_object = NPVARIANT_TO_OBJECT(constructor_variant);
      if (constructor_object != NULL) {
        NPVariant object_variant;
        if (NPN_InvokeDefault(npp, constructor_object, args, numArgs,
                              &object_variant)) {
          if (NPVARIANT_IS_OBJECT(object_variant)) {
            *result = NPVARIANT_TO_OBJECT(object_variant);
            NPN_RetainObject(*result);
            success = true;
          }
          NPN_ReleaseVariantValue(&object_variant);
        }
      }
    }
    NPN_ReleaseVariantValue(&constructor_variant);
  }
  return success;
}

bool NPBrowserProxy::NPN_Evaluate(NPP npp,
                                  NPObject *obj,
                                  NPString *script,
                                  NPVariant *result) {
  if (!npp || !obj || !script) {
    return false;
  }

  NPBrowserProxy *browser_proxy = static_cast<NPBrowserProxy*>(npp->ndata);
  CHostControl *host_control = browser_proxy->GetHostingControl();
  ATLASSERT(host_control);

  NPObject* window_object = browser_proxy->GetVWindowObject();
  if (obj != window_object) {
    return false;
  }

  // Causing IE to run JavaScript code is straightforward if you don't need the
  // result of the evaluation. You can just call IHTMLWindow::execScript. It
  // does not return a valid result though (it specifically says that on MSDN).
  //
  // I tried two other approaches, both of which had the same issue: they didn't
  // return the result. The first unsuccessul approach was to get the "eval"
  // propery of the window object and invoke it as a function with the JS code
  // as the argument. That returns no result. The second unsuccessful approach
  // was to get the "Function" constructor from the window object and invoke it
  // with the JS code as an argument. That should give you a function object and
  // invoking it should evaluate the JS code and return the result. It does not
  // return a result.
  //
  // The final approach (which worked) is to create a Function that additionally
  // takes a temporary object as an argument. The JS code is modified to assign
  // its result to a property of that temporary object called "result". After
  // evaluating the function, the result can then be retrieved from the
  // temporary object.
  bool success = false;
  NPObject* result_object;
  if (ConstructObject(npp, window_object, "Object", NULL, 0, &result_object)) {
    CStringA function_code;
    function_code.Format("result_object.result = (%s);",
                         script->utf8characters);

    NPVariant args[2];
    STRINGZ_TO_NPVARIANT("result_object", args[0]);
    STRINGN_TO_NPVARIANT(function_code.GetBuffer(),
                         (uint32_t) function_code.GetLength(),
                         args[1]);
    NPObject* function_object;
    if (ConstructObject(npp, window_object, "Function", args, 2,
                        &function_object)) {
      OBJECT_TO_NPVARIANT(result_object, args[0]);
      NPVariant dummy_result;
      if (NPN_InvokeDefault(npp, function_object, args, 1, &dummy_result)) {
        NPIdentifier result_identifier = NPN_GetStringIdentifier("result");
        if (NPN_GetProperty(npp, result_object, result_identifier, result)) {
          success = true;
        }
        NPN_ReleaseVariantValue(&dummy_result);
      }
      NPN_ReleaseObject(function_object);
    }
    NPN_ReleaseObject(result_object);
  }
  return success;
}

void NPBrowserProxy::NPN_SetException(NPObject *obj,
                                      const NPUTF8 *message) {
  ATLASSERT(false && "NPN_SetException not implemented");
}

NPNetscapeFuncs* NPBrowserProxy::GetBrowserFunctions() {
  return &kNetscapeFunctions;
}

void NPBrowserProxy::TearDown() {
  // All NPObjectProxy instances stored in the java-script environment must
  // be marked so that scripted operations on these operations fail after
  // the plug-in has been torn down.  We release the hosted object on all of
  // these wrappers to prevent access, and allow deletion of the NPAPI objects.
  NPObjectProxyMap::iterator np_object_iter(np_object_proxy_map_.begin()),
      np_object_end(np_object_proxy_map_.end());
  for (; np_object_iter != np_object_end; ++np_object_iter) {
    np_object_iter->second->ReleaseHosted();
  }
}
