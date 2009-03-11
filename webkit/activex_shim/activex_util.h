// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_ACTIVEX_SHIM_ACTIVEX_UTIL_H__
#define WEBKIT_ACTIVEX_SHIM_ACTIVEX_UTIL_H__

#include <windows.h>
#include <comdef.h>
#include <string>
#include "base/logging.h"
#include "webkit/glue/plugins/nphostapi.h"

namespace activex_shim {

class DispatchObject;

// Logging
#ifdef TRACK_INTERFACE
#define TRACK_METHOD() LOG(INFO) << "Called: " << __FUNCTION__
#define TRACK_QUERY_INTERFACE(iid, succeeded) \
    TrackQueryInterface(iid, succeeded, __FUNCTION__)
#else
#define TRACK_METHOD()
#define TRACK_QUERY_INTERFACE(iid, succeeded)
#endif

// Unfortunately this value is not defined in any Windows header.
const int kHimetricPerInch = 2540;

// Used in macro to log which interface is queried and if it is successful.
void TrackQueryInterface(REFIID iid, bool succeeded, const char* from_function);

// NP types conversions
bool NPIdentifierToWString(NPIdentifier name, std::wstring* ret);
bool VariantToNPVariant(DispatchObject* obj, const VARIANT* vt, NPVariant* npv);
bool NPVariantToVariant(const NPVariant* npv, VARIANT* vt);

// Dispatch interface helpers
bool DispGetID(IDispatch* disp, const wchar_t* name, DISPID* dispid);
bool DispIsMethodOrProperty(IDispatch* disp, const wchar_t* name,
                            bool checkmethod);
// This is a general invoke function. Use this function to call methods or
// get properties.
// DO NOT use this function to set properties. Use DispSetProperty instead.
bool DispInvoke(IDispatch* disp, const wchar_t* name, VARIANT* args,
                int arg_count, VARIANT* result);
// A special version for PROPERTYSET.
bool DispSetProperty(IDispatch* disp, const wchar_t* name,
                     const VARIANT& rvalue);

// ActiveX object security
enum ActiveXSafety {
  SAFE_FOR_SCRIPTING = 0x1,
  SAFE_FOR_INITIALIZING = 0x2,
};

// Gets the IObjectSafety interface of the control and set its safe options.
unsigned long GetAndSetObjectSafetyOptions(IUnknown* control);
// Uses the StdComponentCategoriesMgr to determine the safety options the object
// registered.
unsigned long GetRegisteredObjectSafetyOptions(const CLSID& clsid);

// Coord transformation
// Screen coord to Himetric coord.
long ScreenToHimetricX(long x);
long ScreenToHimetricY(long y);
void ScreenToHimetric(long cx, long cy, SIZE* size);

// Create a copy of the string with memory allocated by CoTaskMemAlloc
wchar_t* CoTaskMemAllocString(const std::wstring& s);

// Reference counted IUnknown implementation.
template <class Base> class IUnknownImpl : public Base {
 public:
  IUnknownImpl() : ref_(1) {
  }
  // IUnknown
  virtual ULONG STDMETHODCALLTYPE AddRef() { return ++ref_; }
  virtual ULONG STDMETHODCALLTYPE Release() {
    --ref_;
    if (ref_ == 0) {
      delete this;
    }
  }
  // We don't add QueryInterface here cause normally the subclass should
  // have its own implementation.

 private:
  ULONG ref_;
};

// The original CComObject does reference counting and delete object when
// reference count reaches 0. This is not desirable for us. If an ActiveX
// control incorrectly decrease our reference, then we will crash. Thus
// let's manage our own life!
template <class Base> class NoRefIUnknownImpl : public Base {
 public:
  ~NoRefIUnknownImpl() {
    // Let the base class clean up before destruction. It's dangerous for base
    // class to do cleanup in destructor, because we usually create the
    // object as: NoRefIUnknownImpl<WebActiveXSite>. So when it
    // destructs, the outer NoRefIUnknownImpl destructs first, then virtual
    // table pointer of IUnknown interface is modified. At this time if we call
    // control's code like IOleInPlaceObject::InPlaceDeactivate, and it calls
    // back to IUnknown of myself, it will cause "pure function call" exception.
    //
    // Using a FinalRelease is what ATL does. I found the reason after getting
    // the crashes in base class' destructor.
    FinalRelease();
  }
  // IUnknown
  virtual ULONG STDMETHODCALLTYPE AddRef() { return 1; }
  virtual ULONG STDMETHODCALLTYPE Release() { return 0; }
  // We don't add QueryInterface here cause normally the subclass should
  // have its own implementation.
};

// A Minimum IDispatch implementation. Used for other classes who need
// the interface but lazy to implement all the typeinfo etc.
class MinimumIDispatchImpl : public IDispatch {
 public:
  virtual HRESULT STDMETHODCALLTYPE GetTypeInfoCount(UINT* ctinfo) {
    *ctinfo = 0;
    return S_OK;
  }
  virtual HRESULT STDMETHODCALLTYPE GetTypeInfo(UINT itinfo, LCID lcid,
                                                ITypeInfo** tinfo) {
    return E_NOTIMPL;
  }
  virtual HRESULT STDMETHODCALLTYPE GetIDsOfNames(
      REFIID riid,
      LPOLESTR* names,
      UINT cnames,
      LCID lcid,
      DISPID* dispids) {
    return E_NOTIMPL;
  }
  virtual HRESULT STDMETHODCALLTYPE Invoke(
      DISPID dispid,
      REFIID riid,
      LCID lcid,
      WORD flags,
      DISPPARAMS* params,
      VARIANT* result,
      EXCEPINFO* except_info,
      UINT* arg_error) {
    return E_NOTIMPL;
  }
};

// This struct is a simple wrap of VARIANT type, so that it could automatically
// initialize when constructed and clear when destructed.
// DO NOT add any virtual function or variable members to this struct, because
// it could be used in arrays.
struct ScopedVariant : public VARIANT {
  ScopedVariant() {
    VariantInit(this);
  }
  ~ScopedVariant() {
    VariantClear(this);
  }
};

}  // namespace activex_shim

#endif // #ifndef WEBKIT_ACTIVEX_SHIM_ACTIVEX_UTIL_H__
