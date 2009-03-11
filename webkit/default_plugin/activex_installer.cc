// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/default_plugin/activex_installer.h"
#include "base/string_util.h"

ActiveXInstaller::ActiveXInstaller()
    : wnd_(NULL),
      notification_msg_(0) {
}

void ActiveXInstaller::Cleanup() {
  if (bind_ctx_ != NULL) {
    RevokeBindStatusCallback(bind_ctx_, this);
    bind_ctx_.Release();
  }
}

HRESULT ActiveXInstaller::StartDownload(const std::string& clsid,
                                        const std::string& codebase,
                                        HWND wnd,
                                        UINT notification_msg) {
  wnd_ = wnd;
  notification_msg_ = notification_msg;

  HRESULT hr = E_FAIL;
  do {
    CLSID id;
    hr = CLSIDFromString(const_cast<LPOLESTR>(ASCIIToWide(clsid).c_str()), &id);
    if (FAILED(hr))
      break;

    // Create the bind context, register it with myself (status callback).
    hr = CreateBindCtx(0, &bind_ctx_);
    if (FAILED(hr))
      break;
    BIND_OPTS opts;
    opts.cbStruct = sizeof(opts);
    bind_ctx_->GetBindOptions(&opts);
    opts.grfFlags |= BIND_MAYBOTHERUSER;
    bind_ctx_->SetBindOptions(&opts);

    hr = RegisterBindStatusCallback(bind_ctx_, this, 0, 0);
    if (FAILED(hr))
      break;
    CComPtr<IClassFactory> class_factory;
    hr = CoGetClassObjectFromURL(id, ASCIIToWide(codebase).c_str(), 0xffffffff,
                                 0xffffffff, NULL, bind_ctx_,
                                 CLSCTX_INPROC_HANDLER | CLSCTX_INPROC_SERVER,
                                 0, IID_IClassFactory, (void**)&class_factory);
  } while(false);

  switch (hr) {
    case S_OK:
      PostMessage(wnd_, notification_msg_, hr, 0);
      break;
    case MK_S_ASYNCHRONOUS:
      // Still need to wait until IBindStatusCallback is updated.
      break;
    default:
      PostMessage(wnd_, notification_msg_, hr, 0);
      break;
  }
  return hr;
}

HRESULT STDMETHODCALLTYPE ActiveXInstaller::OnStartBinding(DWORD dw_reserved,
                                                           IBinding* pib) {
  return S_OK;
}

HRESULT STDMETHODCALLTYPE ActiveXInstaller::GetPriority(LONG* pn_priority) {
  return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE ActiveXInstaller::OnLowResource(DWORD reserved) {
  return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE ActiveXInstaller::OnProgress(ULONG ul_progress,
                                                       ULONG ul_progress_max,
                                                       ULONG ul_status_code,
                                                       LPCWSTR sz_status_text) {
  return S_OK;
}

HRESULT STDMETHODCALLTYPE ActiveXInstaller::OnStopBinding(HRESULT hresult,
                                                          LPCWSTR sz_error) {
  if (wnd_)
    PostMessage(wnd_, notification_msg_, hresult, 0);
  return S_OK;
}

HRESULT STDMETHODCALLTYPE ActiveXInstaller::GetBindInfo(DWORD* grf_bindf,
                                                        BINDINFO* pbindinfo) {
  return S_OK;
}

HRESULT STDMETHODCALLTYPE ActiveXInstaller::OnDataAvailable(
    DWORD grf_bscf,
    DWORD dw_size,
    FORMATETC* pformatetc,
    STGMEDIUM* pstgmed) {
  return S_OK;
}

HRESULT STDMETHODCALLTYPE ActiveXInstaller::OnObjectAvailable(REFIID riid,
                                                              IUnknown* punk) {
  return S_OK;
}

HRESULT STDMETHODCALLTYPE ActiveXInstaller::GetWindow(REFGUID rguid_reason,
                                                      HWND* phwnd) {
  *phwnd = wnd_;
  return S_OK;
}
