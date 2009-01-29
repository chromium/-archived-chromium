// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>
#include <urlmon.h>

#include "chrome/installer/util/html_dialog.h"

#pragma comment(lib, "urlmon.lib")

namespace {
// Signature of MSHTML.DLL ShowHTMLDlg.
typedef HRESULT (CALLBACK *ShowHTMLDlg)(HWND parent_hwnd,
                                        IMoniker *moniker,
                                        VARIANT *in_args,
                                        TCHAR *options,
                                        VARIANT *out_args);
}  // namespace.

namespace installer {

// Windows implementation of the HTML dialog class. The main danger with
// using the IE embedded control as a child window of a custom window is that
// it still contains too much browser functionality, allowing the user to do
// things that are not expected of a plain dialog. ShowHTMLDialog API solves
// that problem but gives us a not very customizable frame. We solve that
// using hooks to end up with a robust dialog at the expense of having to do
// the buttons in html itself, like so:
//
// <form onsubmit="submit_it(this); return false;">
//  <input name="accept" type="checkbox" /> My cool option
//  <input name="submit" type="submit" value="[accept]" />
// </form>
// 
// function submit_it(f) {
//  if (f.accept.checked) {
//    window.returnValue = 1;  <-- this matches HTML_DLG_ACCEPT
//  } else {
//    window.returnValue = 2;  <-- this matches HTML_DLG_DECLINE
//  }
//  window.close();
// }
//
// Note that on the submit handler you need to set window.returnValue to one of
// the values of DialogResult and call window.close().

class HTMLDialogWin : public HTMLDialog {
 public:
  HTMLDialogWin(const std::wstring& url) : url_(url) {
    if (!mshtml_)
       mshtml_ = LoadLibrary(L"MSHTML.DLL");
  }

  virtual DialogResult ShowModal(void* parent_window,
                                 CustomizationCallback* callback) {
    int result = HTML_DLG_DECLINE;
    if (!InternalDoDialog(callback, &result))
      return HTML_DLG_ERROR;
    return static_cast<DialogResult>(result);
  }

  // TODO(cpu): Not yet implemented.
  virtual std::wstring GetExtraResult() {
    return std::wstring();
  }

 private:
  bool InternalDoDialog(CustomizationCallback* callback, int* result);
  static LRESULT CALLBACK MsgFilter(int code, WPARAM wParam, LPARAM lParam);

  std::wstring url_;
  static HHOOK hook_;
  static HINSTANCE mshtml_;
  static CustomizationCallback* callback_;
};

HTMLDialog* CreateNativeHTMLDialog(const std::wstring& url) {
  return new HTMLDialogWin(url);
}

HHOOK HTMLDialogWin::hook_ = NULL;
HINSTANCE HTMLDialogWin::mshtml_ = NULL;
HTMLDialogWin::CustomizationCallback* HTMLDialogWin::callback_ = NULL;

// This hook function gets called for messages bound to the windows that
// ShowHTMLDialog creates. We tell apart the top window because it has the
// system menu style. 
LRESULT HTMLDialogWin::MsgFilter(int code, WPARAM wParam, LPARAM lParam) {
  static bool tweak_window = true;
  if (lParam && tweak_window) {
    HWND target_window = reinterpret_cast<MSG*>(lParam)->hwnd;
    if (target_window) {
      LONG_PTR style = ::GetWindowLongPtrW(target_window, GWL_STYLE);
      if (style & WS_SYSMENU) {
        tweak_window = false;
        callback_->OnBeforeDisplay(target_window);
      }
    }
  }
  // Always call the next hook in the chain.
  return ::CallNextHookEx(hook_, code, wParam, lParam);
}

bool HTMLDialogWin::InternalDoDialog(CustomizationCallback* callback,
                                     int* result) {
  if (!mshtml_)
    return false;
  ShowHTMLDlg show_html_dialog =
      reinterpret_cast<ShowHTMLDlg>(GetProcAddress(mshtml_, "ShowHTMLDialog"));
  if (!show_html_dialog)
    return false;

  IMoniker *url_moniker = NULL;
  ::CreateURLMoniker(NULL, url_.c_str(), &url_moniker);
  if (!url_moniker)  
    return false;

  wchar_t* extra_args = NULL;
  if (callback) {
    callback->OnBeforeCreation(reinterpret_cast<void**>(&extra_args));
    // Sets a windows hook for this thread only.
    hook_ = ::SetWindowsHookEx(WH_GETMESSAGE, MsgFilter, NULL,
                               GetCurrentThreadId());
    if (hook_)
      callback_ = callback;
  }

  VARIANT v_result;
  ::VariantInit(&v_result);

  // Creates the window with the embedded IE control in a modal loop.
  HRESULT hr = show_html_dialog(NULL, url_moniker, NULL, extra_args, &v_result);
  url_moniker->Release();

  if (v_result.vt == VT_I4)
    *result = v_result.intVal;
  ::VariantClear(&v_result);

  if (hook_) {
    ::UnhookWindowsHookEx(hook_);
    callback_ = NULL;
    hook_ = NULL;
  }
  return SUCCEEDED(hr);
}

// EulaHTMLDialog implementation ---------------------------------------------

void EulaHTMLDialog::Customizer::OnBeforeCreation(void** extra) {
}

// The customization of the window consists in removing the close button and
// replacing the existing 'e' icon with the standard informational icon.
void EulaHTMLDialog::Customizer::OnBeforeDisplay(void* window) {
  if (!window)
    return;
  HWND top_window = static_cast<HWND>(window);
  LONG_PTR style = ::GetWindowLongPtrW(top_window, GWL_STYLE);
  ::SetWindowLongPtrW(top_window, GWL_STYLE, style & ~WS_SYSMENU);
  HICON ico = ::LoadIcon(NULL, IDI_INFORMATION);
  ::SendMessageW(top_window, WM_SETICON, ICON_SMALL,
                 reinterpret_cast<LPARAM>(ico));
}

EulaHTMLDialog::EulaHTMLDialog(const std::wstring& file) {
  dialog_ = CreateNativeHTMLDialog(file);
}

EulaHTMLDialog::~EulaHTMLDialog() {
  delete dialog_;
}

bool EulaHTMLDialog::ShowModal() {
  Customizer customizer;
  HTMLDialog::DialogResult dr = dialog_->ShowModal(NULL, &customizer);
  return (HTMLDialog::HTML_DLG_ACCEPT == dr || HTMLDialog::HTML_DLG_EXTRA == dr);
}

}  // namespace installer

