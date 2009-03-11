// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_DEFAULT_PLUGIN_ACTIVEX_INSTALLER_H__
#define WEBKIT_DEFAULT_PLUGIN_ACTIVEX_INSTALLER_H__

#include <atlbase.h>
#include <atlcom.h>
#include <windows.h>
#include <string>

// ActiveXInstaller is to help install an ActiveX control from a URL, usually
// given by codebase.
class ActiveXInstaller : public CComObjectRootEx<CComMultiThreadModel>,
                         public IBindStatusCallback,
                         public IWindowForBindingUI {
 public:
  ActiveXInstaller();

  // Start download and installation for an ActiveX control. After download
  // installation, the installer will send notification_msg to wnd, where
  // WPARAM of the message denotes the HRESULT.
  HRESULT StartDownload(const std::string& clsid, const std::string& codebase,
                        HWND wnd, UINT notification_msg);
  // Revoke binding and release it if it's created.
  void Cleanup();

  // IBindStatusCallback
  virtual HRESULT STDMETHODCALLTYPE OnStartBinding(DWORD dw_reserved,
                                                   IBinding* pib);
  virtual HRESULT STDMETHODCALLTYPE GetPriority(LONG* pn_priority);
  virtual HRESULT STDMETHODCALLTYPE OnLowResource(DWORD reserved);
  virtual HRESULT STDMETHODCALLTYPE OnProgress(ULONG ul_progress,
                                               ULONG ul_progress_max,
                                               ULONG ul_status_code,
                                               LPCWSTR sz_status_text);
  virtual HRESULT STDMETHODCALLTYPE OnStopBinding(HRESULT hresult,
                                                  LPCWSTR sz_error);
  virtual HRESULT STDMETHODCALLTYPE GetBindInfo(DWORD* grf_bindf,
                                                BINDINFO* pbindinfo);
  virtual HRESULT STDMETHODCALLTYPE OnDataAvailable(DWORD grf_bscf,
                                                    DWORD dw_size,
                                                    FORMATETC* pformatetc,
                                                    STGMEDIUM* pstgmed);
  virtual HRESULT STDMETHODCALLTYPE OnObjectAvailable(REFIID riid,
                                                      IUnknown* punk);

  // IWindowForBindingUI
  virtual HRESULT STDMETHODCALLTYPE GetWindow(REFGUID rguid_reason,
                                              HWND* phwnd);

BEGIN_COM_MAP(ActiveXInstaller)
	COM_INTERFACE_ENTRY(IBindStatusCallback)
	COM_INTERFACE_ENTRY(IWindowForBindingUI)
END_COM_MAP()

 private:
  HWND wnd_;
  UINT notification_msg_;
  CComPtr<IBindCtx> bind_ctx_;
};

#endif // #ifndef WEBKIT_DEFAULT_PLUGIN_ACTIVEX_INSTALLER_H__
