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
