// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_ACTIVEX_TEST_CONTROL_CHROME_TEST_CONTROL_H__
#define CHROME_TEST_ACTIVEX_TEST_CONTROL_CHROME_TEST_CONTROL_H__

#include <atlbase.h>
#include <atlcom.h>
#include <atlctl.h>
#include <comutil.h>
#include "activex_test_control.h"
#include "chrome/test/activex_test_control/chrome_test_control_cp.h"
#include "chrome/test/activex_test_control/resource.h"

// ChromeTestControl
class ATL_NO_VTABLE ChromeTestControl
    : public CComObjectRootEx<CComSingleThreadModel>,
      public CStockPropImpl<ChromeTestControl, IChromeTestControl>,
      public IPersistStreamInitImpl<ChromeTestControl>,
      public IOleControlImpl<ChromeTestControl>,
      public IOleObjectImpl<ChromeTestControl>,
      public IOleInPlaceActiveObjectImpl<ChromeTestControl>,
      public IViewObjectExImpl<ChromeTestControl>,
      public IOleInPlaceObjectWindowlessImpl<ChromeTestControl>,
      public ISupportErrorInfo,
      public IConnectionPointContainerImpl<ChromeTestControl>,
      public CProxy_IChromeTestControlEvents<ChromeTestControl>,
      public IObjectWithSiteImpl<ChromeTestControl>,
      public IServiceProviderImpl<ChromeTestControl>,
      public IPersistStorageImpl<ChromeTestControl>,
      public ISpecifyPropertyPagesImpl<ChromeTestControl>,
      public IQuickActivateImpl<ChromeTestControl>,
      public IDataObjectImpl<ChromeTestControl>,
      public IProvideClassInfo2Impl<&CLSID_ChromeTestControl,
          &__uuidof(_IChromeTestControlEvents), &LIBID_activex_test_controlLib>,
      public IPropertyNotifySinkCP<ChromeTestControl>,
      public IObjectSafetyImpl<ChromeTestControl,
                               INTERFACESAFE_FOR_UNTRUSTED_CALLER |
                               INTERFACESAFE_FOR_UNTRUSTED_DATA>,
      public CComCoClass<ChromeTestControl, &CLSID_ChromeTestControl>,
      public CComControl<ChromeTestControl> {
 public:
  ChromeTestControl() {
  }

DECLARE_OLEMISC_STATUS(OLEMISC_RECOMPOSEONRESIZE |
    OLEMISC_CANTLINKINSIDE |
    OLEMISC_INSIDEOUT |
    OLEMISC_ACTIVATEWHENVISIBLE |
    OLEMISC_SETCLIENTSITEFIRST)

DECLARE_REGISTRY_RESOURCEID(IDR_CHROMETESTCONTROL)

BEGIN_COM_MAP(ChromeTestControl)
  COM_INTERFACE_ENTRY(IChromeTestControl)
  COM_INTERFACE_ENTRY(IDispatch)
  COM_INTERFACE_ENTRY(IViewObjectEx)
  COM_INTERFACE_ENTRY(IViewObject2)
  COM_INTERFACE_ENTRY(IViewObject)
  COM_INTERFACE_ENTRY(IOleInPlaceObjectWindowless)
  COM_INTERFACE_ENTRY(IOleInPlaceObject)
  COM_INTERFACE_ENTRY2(IOleWindow, IOleInPlaceObjectWindowless)
  COM_INTERFACE_ENTRY(IOleInPlaceActiveObject)
  COM_INTERFACE_ENTRY(IOleControl)
  COM_INTERFACE_ENTRY(IOleObject)
  COM_INTERFACE_ENTRY(IPersistStreamInit)
  COM_INTERFACE_ENTRY2(IPersist, IPersistStreamInit)
  COM_INTERFACE_ENTRY(ISupportErrorInfo)
  COM_INTERFACE_ENTRY(IConnectionPointContainer)
  COM_INTERFACE_ENTRY(ISpecifyPropertyPages)
  COM_INTERFACE_ENTRY(IQuickActivate)
  COM_INTERFACE_ENTRY(IPersistStorage)
  COM_INTERFACE_ENTRY(IDataObject)
  COM_INTERFACE_ENTRY(IProvideClassInfo)
  COM_INTERFACE_ENTRY(IProvideClassInfo2)
  COM_INTERFACE_ENTRY(IObjectWithSite)
  COM_INTERFACE_ENTRY(IServiceProvider)
  COM_INTERFACE_ENTRY_IID(IID_IObjectSafety, IObjectSafety)
END_COM_MAP()

BEGIN_PROP_MAP(ChromeTestControl)
  PROP_DATA_ENTRY("_cx", m_sizeExtent.cx, VT_UI4)
  PROP_DATA_ENTRY("_cy", m_sizeExtent.cy, VT_UI4)
  PROP_ENTRY_TYPE("BackColor", DISPID_BACKCOLOR, CLSID_StockColorPage,
                  VT_COLOR)
  PROP_ENTRY_TYPE("BorderColor", DISPID_BORDERCOLOR, CLSID_StockColorPage,
                  VT_COLOR)
  PROP_ENTRY_TYPE("Caption", DISPID_CAPTION, CLSID_NULL, VT_BSTR)
  PROP_ENTRY_TYPE("ForeColor", DISPID_FORECOLOR, CLSID_StockColorPage, VT_COLOR)
  // Example entries
  // PROP_ENTRY("Property Description", dispid, clsid)
  // PROP_PAGE(CLSID_StockColorPage)
END_PROP_MAP()

BEGIN_CONNECTION_POINT_MAP(ChromeTestControl)
  CONNECTION_POINT_ENTRY(IID_IPropertyNotifySink)
  CONNECTION_POINT_ENTRY(__uuidof(_IChromeTestControlEvents))
END_CONNECTION_POINT_MAP()

BEGIN_MSG_MAP(ChromeTestControl)
  CHAIN_MSG_MAP(CComControl<ChromeTestControl>)
  DEFAULT_REFLECTION_HANDLER()
END_MSG_MAP()

  // ISupportsErrorInfo
  STDMETHOD(InterfaceSupportsErrorInfo)(REFIID riid) {
    static const IID* arr[] = {
      &IID_IChromeTestControl,
    };

    for (int i=0; i<sizeof(arr)/sizeof(arr[0]); i++) {
      if (InlineIsEqualGUID(*arr[i], riid))
        return S_OK;
    }
    return S_FALSE;
  }

  // IViewObjectEx
  DECLARE_VIEW_STATUS(VIEWSTATUS_SOLIDBKGND | VIEWSTATUS_OPAQUE)

// IChromeTestControl
 public:
  HRESULT OnDraw(ATL_DRAWINFO& di);

  void OnBackColorChanged() {
    ATLTRACE(_T("OnBackColorChanged\n"));
  }
  void OnBorderColorChanged() {
    ATLTRACE(_T("OnBorderColorChanged\n"));
  }
  void OnCaptionChanged() {
    ATLTRACE(_T("OnCaptionChanged\n"));
  }
  void OnForeColorChanged() {
    ATLTRACE(_T("OnForeColorChanged\n"));
  }
  STDMETHOD(_InternalQueryService)(REFGUID guidService, REFIID riid,
                                   void** ppvObject) {
    return E_NOTIMPL;
  }

  DECLARE_PROTECT_FINAL_CONSTRUCT()

  HRESULT FinalConstruct() {
    return S_OK;
  }

  void FinalRelease() {
  }

  STDMETHOD(get_StringProp)(BSTR* val) {
    *val = string_prop_.copy();
    return S_OK;
  }

  STDMETHOD(put_StringProp)(BSTR val) {
    string_prop_ = val;
    return S_OK;
  }
  STDMETHOD(get_LongProp)(LONG* val) {
    *val = long_prop_;
    return S_OK;
  }

  STDMETHOD(put_LongProp)(LONG val) {
    long_prop_ = val;
    return S_OK;
  }
  STDMETHOD(get_DoubleProp)(DOUBLE* val) {
    *val = double_prop_;
    return S_OK;
  }

  STDMETHOD(put_DoubleProp)(DOUBLE val) {
    double_prop_ = val;
    return S_OK;
  }

  STDMETHOD(get_BoolProp)(VARIANT_BOOL* val) {
    *val = bool_prop_;
    return S_OK;
  }

  STDMETHOD(put_BoolProp)(VARIANT_BOOL val) {
    bool_prop_ = val;
    return S_OK;
  }

  STDMETHOD(get_ByteProp)(BYTE* val) {
    *val = byte_prop_;
    return S_OK;
  }

  STDMETHOD(put_ByteProp)(BYTE val) {
    byte_prop_ = val;
    return S_OK;
  }

  STDMETHOD(get_FloatProp)(FLOAT* val) {
    *val = float_prop_;
    return S_OK;
  }

  STDMETHOD(put_FloatProp)(FLOAT val) {
    float_prop_ = val;
    return S_OK;
  }

  STDMETHOD(SetByte)(BYTE val) {
    byte_prop_ = val;
    return S_OK;
  }
  STDMETHOD(SetByteRet)(BYTE val, BYTE* ret) {
    byte_prop_ = val;
    *ret = val;
    return S_OK;
  }
  STDMETHOD(SetStringRet)(BSTR val, BSTR* ret) {
    string_prop_ = val;
    *ret = string_prop_.copy();
    return S_OK;
  }
  STDMETHOD(BigSetMethodRet)(BSTR string_param, BYTE byte_param,
                             FLOAT float_param, VARIANT_BOOL bool_param,
                             BSTR* ret) {
    string_prop_ = string_param;
    byte_prop_ = byte_param;
    float_prop_ = float_param;
    bool_prop_ = bool_param;
    *ret = SysAllocString(string_param);
    return S_OK;
  }
  STDMETHOD(GetCookie)(BSTR* cookie) {
    CComPtr<IOleContainer> container;
    m_spClientSite->GetContainer(&container);
    CComQIPtr<IHTMLDocument2> doc = container;
    if (doc == NULL) {
      *cookie = SysAllocString(L"Bad");
      return S_FALSE;
    } else {
      return doc->get_cookie(cookie);
    }
  }

  // These varialbes are used by CStockPropImpl invisibly and they have to be
  // be public to be accessible.
  OLE_COLOR m_clrBackColor;
  OLE_COLOR m_clrBorderColor;
  CComBSTR m_bstrCaption;
  OLE_COLOR m_clrForeColor;
 private:
  _bstr_t string_prop_;
  LONG long_prop_;
  DOUBLE double_prop_;
  VARIANT_BOOL bool_prop_;
  BYTE byte_prop_;
  FLOAT float_prop_;
};

OBJECT_ENTRY_AUTO(__uuidof(ChromeTestControl), ChromeTestControl)

#endif // #ifndef CHROME_TEST_ACTIVEX_TEST_CONTROL_CHROME_TEST_CONTROL_H__
