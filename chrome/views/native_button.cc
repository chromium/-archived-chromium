// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/views/native_button.h"

#include "base/logging.h"
#include "chrome/common/gfx/chrome_canvas.h"
#include "chrome/common/l10n_util.h"
#include "chrome/common/resource_bundle.h"
#include "chrome/views/background.h"

namespace views {

const char NativeButton::kViewClassName[] = "chrome/views/NativeButton";

NativeButton::NativeButton(const std::wstring& label)
    : enforce_dlu_min_size_(true) {
  Init(label, false);
}

NativeButton::NativeButton(const std::wstring& label, bool is_default)
    : enforce_dlu_min_size_(true) {
  Init(label, is_default);
}

NativeButton::~NativeButton() {
}

std::string NativeButton::GetClassName() const {
  return kViewClassName;
}

void NativeButton::SetListener(Listener *l) {
  listener_ = l;
}

void NativeButton::SetPadding(CSize size) {
  padding_ = size;
}

gfx::Size NativeButton::GetPreferredSize() {
  HWND hwnd = GetNativeControlHWND();
  if (hwnd) {
    SIZE sz = {0, 0};
    ::SendMessage(hwnd,
                  BCM_GETIDEALSIZE,
                  0,
                  reinterpret_cast<LPARAM>(&sz));
    sz.cx += 2 * padding_.cx;
    sz.cy += 2 * padding_.cy;

    if (enforce_dlu_min_size_) {
      if (min_dlu_size_.width()) {
        sz.cx =
            std::max(static_cast<int>(sz.cx),
                     font_.horizontal_dlus_to_pixels(min_dlu_size_.width()));
      }
      if (min_dlu_size_.height())
        sz.cy = std::max(static_cast<int>(sz.cy),
                         font_.vertical_dlus_to_pixels(min_dlu_size_.height()));
    }
    return gfx::Size(sz.cx, sz.cy);
  }
  return gfx::Size();
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
                            width(), height(), parent_container, NULL,
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

void NativeButton::SetDefaultButton(bool is_default_button) {
  if (is_default_button == is_default_)
    return;
  is_default_ = is_default_button;
  if (is_default_button)
    AddAccelerator(Accelerator(VK_RETURN, false, false, false));
  else
    RemoveAccelerator(Accelerator(VK_RETURN, false, false, false));
  SendMessage(GetNativeControlHWND(), BM_SETSTYLE,
              is_default_button ? BS_DEFPUSHBUTTON : BS_PUSHBUTTON, true);
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

}  // namespace views
