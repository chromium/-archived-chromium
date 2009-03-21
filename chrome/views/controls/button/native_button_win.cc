// Copyright (c) 2009 The Chromium Authors. All rights reserved. Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#include "chrome/views/controls/button/native_button_win.h"

#include "base/logging.h"
#include "chrome/views/controls/button/checkbox2.h"
#include "chrome/views/controls/button/native_button2.h"
#include "chrome/views/controls/button/radio_button2.h"
#include "chrome/views/widget/widget.h"

namespace views {

////////////////////////////////////////////////////////////////////////////////
// NativeButtonWin, public:

NativeButtonWin::NativeButtonWin(NativeButton2* native_button)
    : NativeControlWin(),
      native_button_(native_button) {
}

NativeButtonWin::~NativeButtonWin() {
}

////////////////////////////////////////////////////////////////////////////////
// NativeButtonWin, NativeButtonWrapper implementation:

void NativeButtonWin::UpdateLabel() {
  SetWindowText(GetHWND(), native_button_->label().c_str());
}

void NativeButtonWin::UpdateFont() {
  SendMessage(GetHWND(), WM_SETFONT,
              reinterpret_cast<WPARAM>(native_button_->font().hfont()),
              FALSE);
}

void NativeButtonWin::UpdateDefault() {
  if (!IsCheckbox()) {
    SendMessage(GetHWND(), BM_SETSTYLE,
                native_button_->is_default() ? BS_DEFPUSHBUTTON : BS_PUSHBUTTON,
                true);
  }
}

View* NativeButtonWin::GetView() {
  return this;
}

////////////////////////////////////////////////////////////////////////////////
// NativeButtonWin, View overrides:

gfx::Size NativeButtonWin::GetPreferredSize() {
  SIZE sz = {0};
  SendMessage(GetHWND(), BCM_GETIDEALSIZE, 0, reinterpret_cast<LPARAM>(&sz));

  return gfx::Size(sz.cx, sz.cy);
}

////////////////////////////////////////////////////////////////////////////////
// NativeButtonWin, NativeControlWin overrides:

LRESULT NativeButtonWin::ProcessMessage(UINT message,
                                        WPARAM w_param,
                                        LPARAM l_param) {
  if (message == WM_COMMAND && HIWORD(w_param) == BN_CLICKED) {
    native_button_->ButtonPressed();
    return 0;
  }
  return NativeControlWin::ProcessMessage(message, w_param, l_param);
}

bool NativeButtonWin::OnKeyDown(int vkey) {
  bool enter_pressed = vkey == VK_RETURN;
  if (enter_pressed)
    native_button_->ButtonPressed();
  return enter_pressed;
}

bool NativeButtonWin::NotifyOnKeyDown() const {
  return true;
}

void NativeButtonWin::CreateNativeControl() {
  DWORD flags = WS_CHILD | BS_PUSHBUTTON;
  if (native_button_->is_default())
    flags |= BS_DEFPUSHBUTTON;
  HWND control_hwnd = CreateWindowEx(GetAdditionalExStyle(), L"BUTTON", L"",
                                     flags, 0, 0, width(), height(),
                                     GetWidget()->GetNativeView(), NULL, NULL,
                                     NULL);
  NativeControlCreated(control_hwnd);
}

void NativeButtonWin::NativeControlCreated(HWND control_hwnd) {
  NativeControlWin::NativeControlCreated(control_hwnd);

  UpdateFont();
  UpdateLabel();
  UpdateDefault();
}

// We could obtain this from the theme, but that only works if themes are
// active.
static const int kCheckboxSize = 13; // pixels

////////////////////////////////////////////////////////////////////////////////
// NativeCheckboxWin, public:

NativeCheckboxWin::NativeCheckboxWin(Checkbox2* checkbox)
    : NativeButtonWin(checkbox),
      checkbox_(checkbox) {
}

NativeCheckboxWin::~NativeCheckboxWin() {
}

////////////////////////////////////////////////////////////////////////////////
// NativeCheckboxWin, View overrides:

gfx::Size NativeCheckboxWin::GetPreferredSize() {
  return gfx::Size(kCheckboxSize, kCheckboxSize);
}

////////////////////////////////////////////////////////////////////////////////
// NativeCheckboxWin, NativeButtonWrapper implementation:

void NativeCheckboxWin::UpdateChecked() {
  SendMessage(GetHWND(), BM_SETCHECK,
              checkbox_->checked() ? BST_CHECKED : BST_UNCHECKED, 0);
}

void NativeCheckboxWin::SetPushed(bool pushed) {
  SendMessage(GetHWND(), BM_SETSTATE, pushed, 0);
}

////////////////////////////////////////////////////////////////////////////////
// NativeCheckboxWin, NativeButtonWin overrides:

LRESULT NativeCheckboxWin::ProcessMessage(UINT message,
                                          WPARAM w_param,
                                          LPARAM l_param) {
  if (message == WM_COMMAND && HIWORD(w_param) == BN_CLICKED) {
    checkbox_->SetChecked(!checkbox_->checked());
    // Fall through to the NativeButtonWin's handler, which will send the
    // clicked notification to the listener...
  }
  return NativeButtonWin::ProcessMessage(message, w_param, l_param);
}

////////////////////////////////////////////////////////////////////////////////
// NativeCheckboxWin, protected:

void NativeCheckboxWin::CreateNativeControl() {
  HWND control_hwnd = CreateWindowEx(WS_EX_TRANSPARENT | GetAdditionalExStyle(),
                                     L"BUTTON", L"",
                                     WS_CHILD | BS_CHECKBOX | WS_VISIBLE,
                                     0, 0, width(), height(),
                                     GetWidget()->GetNativeView(), NULL, NULL,
                                     NULL);
  NativeControlCreated(control_hwnd);
}

void NativeCheckboxWin::NativeControlCreated(HWND control_hwnd) {
  NativeButtonWin::NativeControlCreated(control_hwnd);
  UpdateChecked();
}

////////////////////////////////////////////////////////////////////////////////
// NativeRadioButtonWin, public:

NativeRadioButtonWin::NativeRadioButtonWin(RadioButton2* radio_button)
    : NativeCheckboxWin(radio_button) {
}

NativeRadioButtonWin::~NativeRadioButtonWin() {
}

////////////////////////////////////////////////////////////////////////////////
// NativeRadioButtonWin, NativeCheckboxWin overrides:

void NativeRadioButtonWin::CreateNativeControl() {
  HWND control_hwnd = CreateWindowEx(GetAdditionalExStyle(), L"BUTTON",
                                     L"", WS_CHILD | BS_RADIOBUTTON,
                                     0, 0, width(), height(),
                                     GetWidget()->GetNativeView(), NULL, NULL,
                                     NULL);
  NativeControlCreated(control_hwnd);
}

////////////////////////////////////////////////////////////////////////////////
// NativeButtonWrapper, public:

// static
NativeButtonWrapper* NativeButtonWrapper::CreateNativeButtonWrapper(
    NativeButton2* native_button) {
  return new NativeButtonWin(native_button);
}

// static
NativeButtonWrapper* NativeButtonWrapper::CreateCheckboxWrapper(
    Checkbox2* checkbox) {
  return new NativeCheckboxWin(checkbox);
}

// static
NativeButtonWrapper* NativeButtonWrapper::CreateRadioButtonWrapper(
    RadioButton2* radio_button) {
  return new NativeRadioButtonWin(radio_button);
}

}  // namespace views

