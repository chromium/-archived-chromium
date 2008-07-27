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

#ifndef WEBKIT_ACTIVEX_SHIM_IWEBBROWSER_IMPL_H__
#define WEBKIT_ACTIVEX_SHIM_IWEBBROWSER_IMPL_H__

#include <exdisp.h>
#include <mshtml.h>
#include "webkit/activex_shim/activex_util.h"

namespace activex_shim {

// Dummy implementation of IWebBrowser2 interface.
class IWebBrowser2Impl : public IWebBrowser2 {
 public:
  // IWebBrowser
  virtual HRESULT STDMETHODCALLTYPE GoBack() {
    TRACK_METHOD();
    return E_NOTIMPL;
  }
  virtual HRESULT STDMETHODCALLTYPE GoForward() {
    TRACK_METHOD();
    return E_NOTIMPL;
  }
  virtual HRESULT STDMETHODCALLTYPE GoHome() {
    TRACK_METHOD();
    return E_NOTIMPL;
  }
  virtual HRESULT STDMETHODCALLTYPE GoSearch() {
    TRACK_METHOD();
    return E_NOTIMPL;
  }
  virtual HRESULT STDMETHODCALLTYPE Navigate(BSTR url,
                                             VARIANT* flags,
                                             VARIANT* target_frame_name,
                                             VARIANT* post_data,
                                             VARIANT* headers) {
    TRACK_METHOD();
    return E_NOTIMPL;
  }
  virtual HRESULT STDMETHODCALLTYPE Refresh() {
    TRACK_METHOD();
    return E_NOTIMPL;
  }
  virtual HRESULT STDMETHODCALLTYPE Refresh2(VARIANT* level) {
    TRACK_METHOD();
    return E_NOTIMPL;
  }
  virtual HRESULT STDMETHODCALLTYPE Stop() {
    TRACK_METHOD();
    return E_NOTIMPL;
  }
  virtual HRESULT STDMETHODCALLTYPE get_Application(IDispatch** disp) {
    TRACK_METHOD();
    return E_NOTIMPL;
  }
  virtual HRESULT STDMETHODCALLTYPE get_Parent(IDispatch** disp) {
    TRACK_METHOD();
    return E_NOTIMPL;
  }
  virtual HRESULT STDMETHODCALLTYPE get_Container(IDispatch** disp) {
    TRACK_METHOD();
    return E_NOTIMPL;
  }
  virtual HRESULT STDMETHODCALLTYPE get_Document(IDispatch** disp) {
    TRACK_METHOD();
    return E_NOTIMPL;
  }
  virtual HRESULT STDMETHODCALLTYPE get_TopLevelContainer(VARIANT_BOOL* b) {
    TRACK_METHOD();
    return E_NOTIMPL;
  }
  virtual HRESULT STDMETHODCALLTYPE get_Type(BSTR* type) {
    TRACK_METHOD();
    return E_NOTIMPL;
  }
  virtual HRESULT STDMETHODCALLTYPE get_Left(long* pl) {
    TRACK_METHOD();
    return E_NOTIMPL;
  }
  virtual HRESULT STDMETHODCALLTYPE put_Left(long left) {
    TRACK_METHOD();
    return E_NOTIMPL;
  }
  virtual HRESULT STDMETHODCALLTYPE get_Top(long* pl) {
    TRACK_METHOD();
    return E_NOTIMPL;
  }
  virtual HRESULT STDMETHODCALLTYPE put_Top(long top) {
    TRACK_METHOD();
    return E_NOTIMPL;
  }
  virtual HRESULT STDMETHODCALLTYPE get_Width(long* pl) {
    TRACK_METHOD();
    return E_NOTIMPL;
  }
  virtual HRESULT STDMETHODCALLTYPE put_Width(long width) {
    TRACK_METHOD();
    return E_NOTIMPL;
  }
  virtual HRESULT STDMETHODCALLTYPE get_Height(long* pl) {
    TRACK_METHOD();
    return E_NOTIMPL;
  }
  virtual HRESULT STDMETHODCALLTYPE put_Height(long height) {
    TRACK_METHOD();
    return E_NOTIMPL;
  }
  virtual HRESULT STDMETHODCALLTYPE get_LocationName(BSTR* location_name) {
    TRACK_METHOD();
    return E_NOTIMPL;
  }
  virtual HRESULT STDMETHODCALLTYPE get_LocationURL(BSTR* location_url) {
    TRACK_METHOD();
    return E_NOTIMPL;
  }
  virtual HRESULT STDMETHODCALLTYPE get_Busy(VARIANT_BOOL* b) {
    TRACK_METHOD();
    return E_NOTIMPL;
  }

  // IWebBrowserApp
  virtual HRESULT STDMETHODCALLTYPE Quit() {
    TRACK_METHOD();
    return E_NOTIMPL;
  }
  virtual HRESULT STDMETHODCALLTYPE ClientToWindow(int* pcx, int* pcy) {
    TRACK_METHOD();
    return E_NOTIMPL;
  }
  virtual HRESULT STDMETHODCALLTYPE PutProperty(BSTR property,
                                                VARIANT vt_value) {
    TRACK_METHOD();
    return E_NOTIMPL;
  }
  virtual HRESULT STDMETHODCALLTYPE GetProperty(BSTR property,
                                                VARIANT* pvt_value) {
    TRACK_METHOD();
    return E_NOTIMPL;
  }
  virtual HRESULT STDMETHODCALLTYPE get_Name(BSTR* name) {
    TRACK_METHOD();
    return E_NOTIMPL;
  }
  virtual HRESULT STDMETHODCALLTYPE get_HWND(SHANDLE_PTR* hwnd) {
    TRACK_METHOD();
    return E_NOTIMPL;
  }
  virtual HRESULT STDMETHODCALLTYPE get_FullName(BSTR* full_name) {
    TRACK_METHOD();
    return E_NOTIMPL;
  }
  virtual HRESULT STDMETHODCALLTYPE get_Path(BSTR* path) {
    TRACK_METHOD();
    return E_NOTIMPL;
  }
  virtual HRESULT STDMETHODCALLTYPE get_Visible(VARIANT_BOOL* b) {
    TRACK_METHOD();
    return E_NOTIMPL;
  }
  virtual HRESULT STDMETHODCALLTYPE put_Visible(VARIANT_BOOL value) {
    TRACK_METHOD();
    return E_NOTIMPL;
  }
  virtual HRESULT STDMETHODCALLTYPE get_StatusBar(VARIANT_BOOL* b) {
    TRACK_METHOD();
    return E_NOTIMPL;
  }
  virtual HRESULT STDMETHODCALLTYPE put_StatusBar(VARIANT_BOOL value) {
    TRACK_METHOD();
    return E_NOTIMPL;
  }
  virtual HRESULT STDMETHODCALLTYPE get_StatusText(BSTR* status_text) {
    TRACK_METHOD();
    return E_NOTIMPL;
  }
  virtual HRESULT STDMETHODCALLTYPE put_StatusText(BSTR status_text) {
    TRACK_METHOD();
    return E_NOTIMPL;
  }
  virtual HRESULT STDMETHODCALLTYPE get_ToolBar(int* value) {
    TRACK_METHOD();
    return E_NOTIMPL;
  }
  virtual HRESULT STDMETHODCALLTYPE put_ToolBar(int value) {
    TRACK_METHOD();
    return E_NOTIMPL;
  }
  virtual HRESULT STDMETHODCALLTYPE get_MenuBar(VARIANT_BOOL* value) {
    TRACK_METHOD();
    return E_NOTIMPL;
  }
  virtual HRESULT STDMETHODCALLTYPE put_MenuBar(VARIANT_BOOL value) {
    TRACK_METHOD();
    return E_NOTIMPL;
  }
  virtual HRESULT STDMETHODCALLTYPE get_FullScreen(
      VARIANT_BOOL* pb_full_screen) {
    TRACK_METHOD();
    return E_NOTIMPL;
  }
  virtual HRESULT STDMETHODCALLTYPE put_FullScreen(VARIANT_BOOL b_full_screen) {
    TRACK_METHOD();
    return E_NOTIMPL;
  }

  // IWebBrowser2
  virtual HRESULT STDMETHODCALLTYPE Navigate2(VARIANT* url,
                                              VARIANT* flags,
                                              VARIANT* target_frame_name,
                                              VARIANT* post_data,
                                              VARIANT* headers) {
    TRACK_METHOD();
    return E_NOTIMPL;
  }
  virtual HRESULT STDMETHODCALLTYPE QueryStatusWB(OLECMDID cmd_id,
                                                  OLECMDF* pcmdf) {
    TRACK_METHOD();
    return E_NOTIMPL;
  }
  virtual HRESULT STDMETHODCALLTYPE ExecWB(OLECMDID cmd_id,
                                           OLECMDEXECOPT cmdexecopt,
                                           VARIANT* pva_in,
                                           VARIANT* pva_out) {
    TRACK_METHOD();
    return E_NOTIMPL;
  }
  virtual HRESULT STDMETHODCALLTYPE ShowBrowserBar(VARIANT* pva_clsid,
                                                   VARIANT* pvar_show,
                                                   VARIANT* pvar_size) {
    TRACK_METHOD();
    return E_NOTIMPL;
  }
  virtual HRESULT STDMETHODCALLTYPE get_ReadyState(READYSTATE* pl_ready_state) {
    TRACK_METHOD();
    return E_NOTIMPL;
  }
  virtual HRESULT STDMETHODCALLTYPE get_Offline(VARIANT_BOOL* pb_offline) {
    TRACK_METHOD();
    return E_NOTIMPL;
  }
  virtual HRESULT STDMETHODCALLTYPE put_Offline(VARIANT_BOOL b_offline) {
    TRACK_METHOD();
    return E_NOTIMPL;
  }
  virtual HRESULT STDMETHODCALLTYPE get_Silent(VARIANT_BOOL* pb_silent) {
    TRACK_METHOD();
    return E_NOTIMPL;
  }
  virtual HRESULT STDMETHODCALLTYPE put_Silent(VARIANT_BOOL b_silent) {
    TRACK_METHOD();
    return E_NOTIMPL;
  }
  virtual HRESULT STDMETHODCALLTYPE get_RegisterAsBrowser(
      VARIANT_BOOL* pb_register) {
    TRACK_METHOD();
    return E_NOTIMPL;
  }
  virtual HRESULT STDMETHODCALLTYPE put_RegisterAsBrowser(
      VARIANT_BOOL b_register) {
    TRACK_METHOD();
    return E_NOTIMPL;
  }
  virtual HRESULT STDMETHODCALLTYPE get_RegisterAsDropTarget(
      VARIANT_BOOL* pb_register) {
    TRACK_METHOD();
    return E_NOTIMPL;
  }
  virtual HRESULT STDMETHODCALLTYPE put_RegisterAsDropTarget(
      VARIANT_BOOL b_register) {
    TRACK_METHOD();
    return E_NOTIMPL;
  }
  virtual HRESULT STDMETHODCALLTYPE get_TheaterMode(VARIANT_BOOL* pb_register) {
    TRACK_METHOD();
    return E_NOTIMPL;
  }
  virtual HRESULT STDMETHODCALLTYPE put_TheaterMode(VARIANT_BOOL b_register) {
    TRACK_METHOD();
    return E_NOTIMPL;
  }
  virtual HRESULT STDMETHODCALLTYPE get_AddressBar(VARIANT_BOOL* value) {
    TRACK_METHOD();
    return E_NOTIMPL;
  }
  virtual HRESULT STDMETHODCALLTYPE put_AddressBar(VARIANT_BOOL value) {
    TRACK_METHOD();
    return E_NOTIMPL;
  }
  virtual HRESULT STDMETHODCALLTYPE get_Resizable(VARIANT_BOOL* value) {
    TRACK_METHOD();
    return E_NOTIMPL;
  }
  virtual HRESULT STDMETHODCALLTYPE put_Resizable(VARIANT_BOOL value) {
    TRACK_METHOD();
    return E_NOTIMPL;
  }
};

}  // namespace activex_shim

#endif // #ifndef WEBKIT_ACTIVEX_SHIM_IWEBBROWSER_IMPL_H__
