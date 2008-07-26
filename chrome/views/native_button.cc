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

#include "chrome/views/native_button.h"

#include "chrome/common/l10n_util.h"
#include "chrome/common/resource_bundle.h"
#include "chrome/views/background.h"

namespace ChromeViews {

NativeButton::NativeButton(const std::wstring& label) {
  Init(label, false);
}

NativeButton::NativeButton(const std::wstring& label, bool is_default) {
  Init(label, is_default);
}

NativeButton::~NativeButton() {
}

void NativeButton::SetListener(Listener *l) {
  listener_ = l;
}

void NativeButton::SetPadding(CSize size) {
  padding_ = size;
}

void NativeButton::GetPreferredSize(CSize *out) {
  HWND hwnd = GetNativeControlHWND();
  if (hwnd) {
    SIZE sz = {0, 0};
    ::SendMessage(hwnd,
                  BCM_GETIDEALSIZE,
                  0,
                  reinterpret_cast<LPARAM>(&sz));
    sz.cx += 2 * padding_.cx;
    sz.cy += 2 * padding_.cy;

    if (min_dlu_size_.width())
      sz.cx = std::max(static_cast<int>(sz.cx),
                       font_.horizontal_dlus_to_pixels(min_dlu_size_.width()));
    if (min_dlu_size_.height())
      sz.cy = std::max(static_cast<int>(sz.cy),
                       font_.vertical_dlus_to_pixels(min_dlu_size_.height()));

    *out = sz;
  }
}

void NativeButton::SetLabel(const std::wstring& l) {
  // Even though we create a flipped HWND for a native button when the locale
  // is right-to-left, Windows does not render text for the button using a
  // right-to-left context (perhaps because the parent HWND is not flipped).
  // The result is that RTL strings containing punctuation marks are not
  // displayed properly. For example, the string "...ABC" (where A, B and C are
  // Hebrew characters) is displayed as "ABC..." which is incorrect.
  //
  // In order to overcome this problem, we mark the localized Hebrew strings as
  // RTL strings explicitly (using the appropriate Unicode formatting) so that
  // Windows displays the text correctly regardless of the HWND hierarchy.
  std::wstring localized_label;
  if (l10n_util::AdjustStringForLocaleDirection(l, &localized_label))
    label_.assign(localized_label);
  else
    label_.assign(l);

  SetAccessibleName(l);
  UpdateNativeButton();
}

const std::wstring NativeButton::GetLabel() const {
  return label_;
}

HWND NativeButton::CreateNativeControl(HWND parent_container) {
  DWORD flags = WS_CHILD | BS_PUSHBUTTON;
  if (is_default_)
    flags |= BS_DEFPUSHBUTTON;
  HWND r = ::CreateWindowEx(GetAdditionalExStyle(), L"BUTTON", L"", flags, 0, 0,
                            GetWidth(), GetHeight(), parent_container, NULL,
                            NULL, NULL);
  SendMessage(r, WM_SETFONT, reinterpret_cast<WPARAM>(font_.hfont()), FALSE);
  ConfigureNativeButton(r);
  return r;
}

LRESULT NativeButton::OnNotify(int w_param, LPNMHDR l_param) {
  return 0;
}

LRESULT NativeButton::OnCommand(UINT code, int id, HWND source) {
  if (code == BN_CLICKED)
    Clicked();
  return 0;
}

void NativeButton::UpdateNativeButton() {
  HWND hwnd = GetNativeControlHWND();
  if (hwnd)
    ConfigureNativeButton(hwnd);
}

void NativeButton::ConfigureNativeButton(HWND hwnd) {
  ::SetWindowText(hwnd, label_.c_str());
}

bool NativeButton::AcceleratorPressed(const Accelerator& accelerator) {
  if (enabled_) {
    Clicked();
    return true;
  }
  return false;
}

bool NativeButton::GetAccessibleRole(VARIANT* role) {
  DCHECK(role);

  role->vt = VT_I4;
  role->lVal = ROLE_SYSTEM_PUSHBUTTON;
  return true;
}

bool NativeButton::GetAccessibleName(std::wstring* name) {
  if (!accessible_name_.empty()) {
    *name = accessible_name_;
    return true;
  }
  return false;
}

void NativeButton::SetAccessibleName(const std::wstring& name) {
  accessible_name_.assign(name);
}

void NativeButton::Init(const std::wstring& label, bool is_default) {
  // Marking the string as an RTL string if the locale is RTL. Refer to
  // the comments in NativeButton::SetLabel for more details.
  std::wstring localized_label;
  if (l10n_util::AdjustStringForLocaleDirection(label, &localized_label))
    label_.assign(localized_label);
  else
    label_.assign(label);

  l10n_util::AdjustStringForLocaleDirection(label, &label_);
  listener_ = NULL;
  SetAccessibleName(label);
  // The padding of 8 is a bit arbitrary, there appears to be no way to
  // get a recommended padding, and this value varies greatly among windows
  // dialogs.
  //
  // The min size in DLUs comes from
  // http://msdn.microsoft.com/library/default.asp?url=/library/en-us/dnwue/html/ch14e.asp
  padding_ = CSize(8, 0);
  is_default_ = is_default;
  min_dlu_size_.SetSize(50, 14);
  SetFocusable(true);
  if (is_default)
    AddAccelerator(Accelerator(VK_RETURN, false, false, false));
}

void NativeButton::Clicked() {
  DCHECK(enabled_);
  // Give the focus to the button.
  if (IsFocusable())
    RequestFocus();

  if (listener_)
    listener_->ButtonPressed(this);
}

bool NativeButton::NotifyOnKeyDown() const {
  return true;
}

bool NativeButton::OnKeyDown(int virtual_key_code) {
  if (virtual_key_code == VK_RETURN) {
    Clicked();
    return true;
  }
  return false;
}

}
