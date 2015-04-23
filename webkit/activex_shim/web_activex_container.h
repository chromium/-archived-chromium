// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_ACTIVEX_SHIM_WEB_ACTIVEX_CONTAINER_H__
#define WEBKIT_ACTIVEX_SHIM_WEB_ACTIVEX_CONTAINER_H__

#include <atlbase.h>
#include <atlcom.h>
#include <string>
#include <vector>
#include "webkit/activex_shim/activex_util.h"
#include "webkit/activex_shim/ihtmldocument_impl.h"
#include "webkit/activex_shim/iwebbrowser_impl.h"

namespace activex_shim {

class ActiveXPlugin;
class WebActiveXSite;

// WebActiveXContainer, as the container of the ActiveX control,
// implements the basic interfaces need to manage and interact with ActiveX
// controls.
// Theoretically this container can hold multiple sites/controls. However,
// in our case we only use 1 container per control for now.
//
// IOleContainer:
//   Required interface.
// IOleInPlaceFrame
//   Required interface.
// IHTMLDocument2:
//   WMP will query this interface to get URL.
// IWebBrowser2:
//   Flash will use IWebBrowser::Navigate to open a URL.
// IBindHost:
//   Flash will use this interface to resolve URL.
class WebActiveXContainer : public IOleContainer,
                            public IOleInPlaceFrame,
                            public IHTMLDocument2Impl,
                            public IWebBrowser2Impl,
                            public IBindHost {
 public:
  WebActiveXContainer();
  virtual ~WebActiveXContainer();

  // Initialize container with related ActiveXPlugin.
  void Init(ActiveXPlugin* plugin);
  // Deactivate and release contained controls. called by outer
  // NoRefIUnknownImpl's destructor.
  void FinalRelease();

  // Create ActiveX control together with its site. Do not do any initialization
  // now but we will query the interface that the control can support,
  // to decide whether it is viewable, supports windowless etc.
  HRESULT CreateControlWithSite(const wchar_t* clsid);
  // Called by ActiveXPlugin when it gets browser window first time.
  void set_container_wnd(HWND hwnd) { container_wnd_ = hwnd; }
  HWND container_wnd() { return container_wnd_; }
  // Returns the IUnknown of the first control. NULL if it doesn't have any.
  IUnknown* GetFirstControl();
  // Returns the first site. NULL if it doesn't have any.
  WebActiveXSite* GetFirstSite();
  ActiveXPlugin* plugin() { return plugin_; }

  // The containing Window should call this upon messages. Return true if we
  // consumed the message
  bool OnWindowMessage(UINT msg, WPARAM wparam, LPARAM lparam,
                       LRESULT* result);

  // IUnknown
  virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid,
                                                   void** object);

  // IDispatch
  virtual HRESULT STDMETHODCALLTYPE GetTypeInfoCount(UINT* ctinfo) {
    TRACK_METHOD();
    *ctinfo = 0;
    return S_OK;
  }
  virtual HRESULT STDMETHODCALLTYPE GetTypeInfo(UINT itinfo, LCID lcid,
                                                ITypeInfo** tinfo) {
    TRACK_METHOD();
    return E_NOTIMPL;
  }
  virtual HRESULT STDMETHODCALLTYPE GetIDsOfNames(
      REFIID riid,
      LPOLESTR* names,
      UINT cnames,
      LCID lcid,
      DISPID* dispids) {
    TRACK_METHOD();
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
    TRACK_METHOD();
    return E_NOTIMPL;
  }

  // IParseDisplayName
  virtual HRESULT STDMETHODCALLTYPE ParseDisplayName(
      IBindCtx* bc,
      LPOLESTR display_name,
      ULONG* cheaten,
      IMoniker** moniker);

  // IOleContainer
  virtual HRESULT STDMETHODCALLTYPE EnumObjects(DWORD flags,
      IEnumUnknown** ppenum);
  virtual HRESULT STDMETHODCALLTYPE LockContainer(BOOL lock);

  // IOleWindow
  virtual HRESULT STDMETHODCALLTYPE GetWindow(HWND* wnd);
  virtual HRESULT STDMETHODCALLTYPE ContextSensitiveHelp(BOOL enter_mode);

  // IOleInPlaceUIWindow
  virtual HRESULT STDMETHODCALLTYPE GetBorder(LPRECT border);
  virtual HRESULT STDMETHODCALLTYPE RequestBorderSpace(
      LPCBORDERWIDTHS border_widths);
  virtual HRESULT STDMETHODCALLTYPE SetBorderSpace(
      LPCBORDERWIDTHS border_widths);
  virtual HRESULT STDMETHODCALLTYPE SetActiveObject(
      IOleInPlaceActiveObject* active_object,
      LPCOLESTR obj_name);

  // IOleInPlaceFrame
  virtual HRESULT STDMETHODCALLTYPE InsertMenus(
      HMENU hmenu_shared,
      LPOLEMENUGROUPWIDTHS menu_widths);
  virtual HRESULT STDMETHODCALLTYPE SetMenu(
      HMENU hmenu_shared,
      HOLEMENU hole_menu,
      HWND active_object);
  virtual HRESULT STDMETHODCALLTYPE RemoveMenus(HMENU hmenu_shared);
  virtual HRESULT STDMETHODCALLTYPE SetStatusText(LPCOLESTR status_text);
  virtual HRESULT STDMETHODCALLTYPE EnableModeless(BOOL enable);
  virtual HRESULT STDMETHODCALLTYPE TranslateAccelerator(
      LPMSG msg,
      WORD id);

  // IHTMLDocument2
  virtual HRESULT STDMETHODCALLTYPE get_URL(BSTR* p);
  virtual HRESULT STDMETHODCALLTYPE get_cookie(BSTR* p);

  // IWebBrowser
  virtual HRESULT STDMETHODCALLTYPE Navigate(BSTR url,
                                             VARIANT* flags,
                                             VARIANT* target_frame_name,
                                             VARIANT* post_data,
                                             VARIANT* headers);

  // IWebBrowserApp
  virtual HRESULT STDMETHODCALLTYPE get_LocationURL(BSTR* location_url);

  // IBindHost
  virtual HRESULT STDMETHODCALLTYPE CreateMoniker(
      LPOLESTR szName,
      IBindCtx* bc,
      IMoniker** mk,
      DWORD reserved);
  virtual HRESULT STDMETHODCALLTYPE MonikerBindToStorage(
      IMoniker* mk,
      IBindCtx* bc,
      IBindStatusCallback* bsc,
      REFIID riid,
      void** obj);
  virtual HRESULT STDMETHODCALLTYPE MonikerBindToObject(
      IMoniker* mk,
      IBindCtx* bc,
      IBindStatusCallback* bsc,
      REFIID riid,
      void** obj);

 private:
  ActiveXPlugin* plugin_;
  HWND container_wnd_;
  std::vector<WebActiveXSite*> sites_;

  DISALLOW_EVIL_CONSTRUCTORS(WebActiveXContainer);
};

}  // namespace activex_shim

#endif // #ifndef WEBKIT_ACTIVEX_SHIM_WEB_ACTIVEX_CONTAINER_H__
