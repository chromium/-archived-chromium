// Copyright 2008, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

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
