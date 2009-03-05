// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/activex_shim/activex_util.h"

#include <exdisp.h>
#include <math.h>
#include <ocmm.h>

#include "base/string_util.h"
#include "webkit/activex_shim/npp_impl.h"
#include "webkit/activex_shim/activex_plugin.h"

using std::string;
using std::wstring;

namespace activex_shim {

#ifdef TRACK_INTERFACE

namespace {
struct IIDToName {
  IID iid;
  const char* name;
};
} // namespace

#define MAP_IID_TO_NAME(name) {IID_##name, #name},

// Map frequently used iid to names. If unknown, return "Unknown:{xxxx-..}".
static string MapIIDToName(REFIID iid) {
  IIDToName const well_known_interface_names[] = {
    MAP_IID_TO_NAME(IUnknown)
    MAP_IID_TO_NAME(IDispatch)
    MAP_IID_TO_NAME(IParseDisplayName)
    MAP_IID_TO_NAME(IOleContainer)
    MAP_IID_TO_NAME(IOleWindow)
    MAP_IID_TO_NAME(IOleInPlaceUIWindow)
    MAP_IID_TO_NAME(IOleInPlaceFrame)
    MAP_IID_TO_NAME(IHTMLDocument)
    MAP_IID_TO_NAME(IHTMLDocument2)
    MAP_IID_TO_NAME(IHTMLWindow2)
    MAP_IID_TO_NAME(IOleClientSite)
    MAP_IID_TO_NAME(IOleControlSite)
    MAP_IID_TO_NAME(IOleInPlaceSite)
    MAP_IID_TO_NAME(IOleInPlaceSiteEx)
    MAP_IID_TO_NAME(IOleInPlaceSiteWindowless)
    MAP_IID_TO_NAME(IServiceProvider)
    MAP_IID_TO_NAME(IBindHost)
    MAP_IID_TO_NAME(IWebBrowserApp)
    MAP_IID_TO_NAME(ITimerService)
  };
  for (int i = 0; i < arraysize(well_known_interface_names); ++i) {
    if (well_known_interface_names[i].iid == iid)
      return well_known_interface_names[i].name;
  }
  LPOLESTR sz = NULL;
  StringFromIID(iid, &sz);
  string res = string("Unknown:") + WideToUTF8(sz);
  CoTaskMemFree(sz);
  return res;
}

void TrackQueryInterface(REFIID iid, bool succeeded,
                         const char* from_function) {
  string name = MapIIDToName(iid);
  if (succeeded) {
    LOG(INFO) << "Successfully Queried: " << name << " in " \
              << from_function;
  } else {
    LOG(WARNING) << "Failed to Query: " << name << " in " \
                 << from_function;
  }
}

#endif  // #ifdef TRACK_INTERFACE

bool NPIdentifierToWString(NPIdentifier name, wstring* ret) {
  if (!g_browser->identifierisstring(name))
    return false;

  NPUTF8* str = g_browser->utf8fromidentifier(name);
  *ret = UTF8ToWide(str);
  g_browser->memfree(str);

  return true;
}

bool DispGetID(IDispatch* disp, const wchar_t* name, DISPID* dispid) {
  if (disp == NULL)
    return false;
  HRESULT hr = disp->GetIDsOfNames(IID_NULL, const_cast<LPOLESTR*>(&name), 1,
                                   LOCALE_SYSTEM_DEFAULT, dispid);
  if (FAILED(hr))
    return false;
  return true;
}

// Get the ITypeInfo of the dispatch interface and look for the FUNCDESC of
// the member. If not found or disp is NULL, return false.
static bool DispGetFuncDesc(IDispatch* disp, const wchar_t* name,
                            FUNCDESC* func) {
  if (disp == NULL)
    return false;
  bool res = false;
  CComPtr<ITypeInfo> tpi;
  TYPEATTR* typeattr = NULL;
  do {
    HRESULT hr = disp->GetTypeInfo(0, LOCALE_SYSTEM_DEFAULT, &tpi);
    if (FAILED(hr))
      break;
    hr = tpi->GetTypeAttr(&typeattr);
    if (FAILED(hr))
      break;

    MEMBERID memid;
    hr = tpi->GetIDsOfNames(const_cast<LPOLESTR*>(&name), 1, &memid);
    if (FAILED(hr))
      break;

    for (int i = 0; i < typeattr->cFuncs && !res; i++) {
      FUNCDESC* funcdesc = NULL;
      hr = tpi->GetFuncDesc(i, &funcdesc);
      if (FAILED(hr))
        continue;
      if (memid == funcdesc->memid) {
        *func = *funcdesc;
        res = true;
      }
      tpi->ReleaseFuncDesc(funcdesc);
    }
  } while(false);
  if (tpi && typeattr)
    tpi->ReleaseTypeAttr(typeattr);
  return res;
}

bool DispIsMethodOrProperty(IDispatch* disp, const wchar_t* name,
                            bool checkmethod) {
  FUNCDESC funcdesc;
  if (DispGetFuncDesc(disp, name, &funcdesc)) {
    // If it has multiple params, for PROPERTYGET, we have to treat it like a
    // method, because the scripting engine will not handle properties with
    // parameters.
    bool is_method = funcdesc.invkind == INVOKE_FUNC || funcdesc.cParams > 0;
    return checkmethod == is_method;
  } else {
    // If it does not have a FUNCDESC, it should be a variable (property) if it
    // has a dispid.
    DISPID dispid;
    if (DispGetID(disp, name, &dispid))
      return checkmethod == false;
    else
      return false;
  }
}

bool DispSetProperty(IDispatch* disp, const wchar_t* name,
                     const VARIANT& rvalue) {
  if (disp == NULL)
    return false;

  DISPID dispid;
  if (!DispGetID(disp, name, &dispid))
    return false;
  DISPPARAMS params = {0};
  params.cArgs = 1;
  params.rgvarg = const_cast<VARIANTARG*>(&rvalue);
  DISPID dispid_named = DISPID_PROPERTYPUT;
  params.cNamedArgs = 1;
  params.rgdispidNamedArgs = &dispid_named;

  unsigned int argerr;
  ScopedVariant result;
  HRESULT hr = disp->Invoke(dispid, IID_NULL, LOCALE_SYSTEM_DEFAULT,
                            DISPATCH_PROPERTYPUT, &params, &result, NULL,
                            &argerr);
  return SUCCEEDED(hr);
}

bool DispInvoke(IDispatch* disp, const wchar_t* name, VARIANT* args,
                int arg_count, VARIANT* result) {
  if (disp == NULL)
    return false;

  unsigned int invoke_option;
  DISPID dispid;
  FUNCDESC funcdesc;
  if (DispGetFuncDesc(disp, name, &funcdesc)) {
    dispid = funcdesc.memid;
    switch (funcdesc.invkind) {
      case INVOKE_FUNC:
        invoke_option = DISPATCH_METHOD;
        break;
      case INVOKE_PROPERTYGET:
        invoke_option = DISPATCH_PROPERTYGET;
        break;
      default:
        return false;
    }
  } else {
    // Could be a variable if it doesn't have a FUNCDESC.
    if (DispGetID(disp, name, &dispid))
      invoke_option = DISPATCH_PROPERTYGET;
    else
      return false;
  }
  VariantInit(result);
  DISPPARAMS params = {0};
  params.cArgs = arg_count;
  params.rgvarg = args;
  unsigned int argerr;

  HRESULT hr = disp->Invoke(dispid, IID_NULL, LOCALE_SYSTEM_DEFAULT,
                            invoke_option, &params, result, NULL, &argerr);
  return SUCCEEDED(hr);
}

// If the given interface is safe for the flag, return true. Otherwise
// try to enable the safety option for the flag, return true if succeeded.
bool TestAndSetObjectSafetyOption(IObjectSafety* object_safety,
                                  const IID& iid, DWORD flag) {
  DWORD supported_options = 0, enabled_options = 0;
  HRESULT hr = object_safety->GetInterfaceSafetyOptions(iid,
                                                &supported_options,
                                                &enabled_options);
  if (SUCCEEDED(hr)) {
    if (enabled_options & flag) {
      return true;
    } else if (supported_options & flag) {
      hr = object_safety->SetInterfaceSafetyOptions(iid, flag, flag);
      if (SUCCEEDED(hr))
        return true;
    }
  }
  return false;
}

unsigned long GetAndSetObjectSafetyOptions(IUnknown* control) {
  unsigned long ret = 0;

  // If we have the interface then check that first.
  CComQIPtr<IObjectSafety> object_safety = control;
  if (object_safety != NULL) {
    // Some controls only claim IPersistPropertyBag is safe. The best way
    // would be checking if an interface is safe only when we use it. In reality
    // this is sufficient enough, considering we have a whitelist.
    static IID persist_iids[] = {IID_IPersist, IID_IPersistPropertyBag,
                                 IID_IPersistPropertyBag2};
    for (int i = 0; i < arraysize(persist_iids); ++i) {
      if (TestAndSetObjectSafetyOption(object_safety, persist_iids[i],
                                       INTERFACESAFE_FOR_UNTRUSTED_DATA)) {
        ret |= SAFE_FOR_INITIALIZING;
        break;
      }
    }
    if (TestAndSetObjectSafetyOption(object_safety, IID_IDispatch,
                                     INTERFACESAFE_FOR_UNTRUSTED_CALLER))
      ret |= SAFE_FOR_SCRIPTING;
  }
  return ret;
}

unsigned long GetRegisteredObjectSafetyOptions(const CLSID& clsid) {
  unsigned long ret = 0;
  HRESULT hr;
  CComPtr<ICatInformation> cat_info;
  hr = CoCreateInstance(CLSID_StdComponentCategoriesMgr,
                        NULL, CLSCTX_INPROC_SERVER, IID_ICatInformation,
                        reinterpret_cast<void**>(&cat_info));
  DCHECK(SUCCEEDED(hr));
  if (FAILED(hr))
    return ret;

  hr = cat_info->IsClassOfCategories(clsid, 1, &CATID_SafeForInitializing,
                                     0, NULL);
  if (hr == S_OK)
    ret |= SAFE_FOR_INITIALIZING;
  hr = cat_info->IsClassOfCategories(clsid, 1, &CATID_SafeForScripting,
                                     0, NULL);
  if (hr == S_OK)
    ret |= SAFE_FOR_SCRIPTING;

  return ret;
}

namespace {
// To allow only querying once for most frequently used device caps.
struct DeviceCaps {
  DeviceCaps() {
    inited_ = false;
  }
  long GetLogPixelX() {
    Use();
    return log_pixel_x;
  }
  long GetLogPixelY() {
    Use();
    return log_pixel_y;
  }
 private:
  // Purposely make this inline.
  void Use() {
    if (!inited_)
      Init();
  }
  void Init();

  bool inited_;
  long log_pixel_x;
  long log_pixel_y;
};

void DeviceCaps::Init() {
  HDC dc = ::GetWindowDC(NULL);
  log_pixel_x = ::GetDeviceCaps(dc, LOGPIXELSX);
  log_pixel_y = ::GetDeviceCaps(dc, LOGPIXELSY);
  ::ReleaseDC(NULL, dc);
  inited_ = true;
}
} // namespace

static DeviceCaps g_devicecaps;

long ScreenToHimetricX(long x) {
  return MulDiv(x, kHimetricPerInch, g_devicecaps.GetLogPixelX());
}

long ScreenToHimetricY(long y) {
  return MulDiv(y, kHimetricPerInch, g_devicecaps.GetLogPixelY());
}

void ScreenToHimetric(long cx, long cy, SIZE* size) {
  size->cx = ScreenToHimetricX(cx);
  size->cy = ScreenToHimetricY(cy);
}

bool VariantToNPVariant(DispatchObject* obj, const VARIANT* vt,
                        NPVariant* npv) {
  // TODO(ruijiang): Support more types when necessary.
  switch(vt->vt) {
    case VT_BSTR: {
      npv->type = NPVariantType_String;
      if (vt->bstrVal == NULL) {
        npv->value.stringValue.UTF8Characters = NULL;
        npv->value.stringValue.UTF8Length = 0;
        return true;
      }
      string s;
      s = WideToUTF8(vt->bstrVal);
      // We need to let browser allocate this memory because this will go
      // out of our control and be used/released by the browser.
      npv->value.stringValue.UTF8Characters =
          static_cast<NPUTF8*>(g_browser->memalloc((uint32)s.size()));
      memcpy(static_cast<void*>(const_cast<NPUTF8*>(
                 npv->value.stringValue.UTF8Characters)),
             s.c_str(), s.size());
      npv->value.stringValue.UTF8Length = static_cast<uint32>(s.size());
      return true;
    }
    case VT_DISPATCH: {
      SpawnedDispatchObject* disp_object =
          new SpawnedDispatchObject(vt->pdispVal, obj->root());
      npv->type = NPVariantType_Object;
      npv->value.objectValue = disp_object->GetScriptableNPObject();
      return true;
    }
    // All integer types.
    case VT_I1:
    case VT_I2:
    case VT_I4:
    case VT_INT:
    case VT_UI1:
    case VT_UI2:
    case VT_UI4:
    case VT_UINT: {
      ScopedVariant tmp;
      if (FAILED(VariantChangeType(&tmp, vt, 0, VT_I4)))
        return false;
      npv->type = NPVariantType_Int32;
      npv->value.intValue = tmp.lVal;
      return true;
    }
    // Float number types.
    case VT_I8:
    case VT_UI8:
    case VT_CY:
    case VT_R4:
    case VT_R8: {
      ScopedVariant tmp;
      if (FAILED(VariantChangeType(&tmp, vt, 0, VT_R8)))
        return false;
      npv->type = NPVariantType_Double;
      npv->value.doubleValue = tmp.dblVal;
      return true;
    }
    case VT_BOOL: {
      npv->type = NPVariantType_Bool;
      npv->value.boolValue = vt->boolVal != VARIANT_FALSE;
      return true;
    }
    case VT_NULL: {
      npv->type = NPVariantType_Null;
      return true;
    }
    case VT_EMPTY: {
      npv->type = NPVariantType_Void;
      return true;
    }
    default: {
      return false;
    }
  }
}

bool NPVariantToVariant(const NPVariant* npv, VARIANT* vt) {
  VariantInit(vt);
  switch(npv->type) {
    case NPVariantType_String:
      vt->vt = VT_BSTR;
      if (npv->value.stringValue.UTF8Length > 0) {
        string str;
        str.assign(npv->value.stringValue.UTF8Characters,
                   npv->value.stringValue.UTF8Length);
        vt->bstrVal = SysAllocString(UTF8ToWide(str.c_str()).c_str());
      } else {
        vt->bstrVal = NULL;
      }
      return true;
    case NPVariantType_Int32:
      vt->vt = VT_I4;
      vt->intVal = npv->value.intValue;
      return true;
    case NPVariantType_Double:
      vt->vt = VT_R8;
      vt->dblVal = npv->value.doubleValue;
      return true;
    case NPVariantType_Bool:
      vt->vt = VT_BOOL;
      vt->boolVal = npv->value.boolValue ? VARIANT_TRUE : VARIANT_FALSE;
      return true;
    case NPVariantType_Null:
      vt->vt = VT_NULL;
      return true;
    case NPVariantType_Void:
      // According to: http://developer.mozilla.org/en/docs/NPVariant
      // Void type corresponds to JavaScript type: undefined, which means
      // no value has been assigned. Thus VT_EMPTY is the best variant matches
      // void.
      vt->vt = VT_EMPTY;
      return true;
    case NPVariantType_Object:
      // TODO(ruijiang): We probably need to convert NP object to IDispatch,
      // which would be a pretty hard job. (consider the many interfaces of
      // IHTML*).
      return false;
    default:
      return false;
  }
}

wchar_t* CoTaskMemAllocString(const std::wstring& s) {
  size_t cb = (s.size() + 1) * sizeof(wchar_t);
  LPOLESTR p = static_cast<LPOLESTR>(CoTaskMemAlloc(cb));
  memcpy(p, s.c_str(), cb);
  return p;
}

}  // namespace activex_shim

