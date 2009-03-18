// Copyright (c) 2009 The Chromium Authors. All rights reserved. Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#include "chrome/views/controls/button/native_button_win.h"

#include "chrome/common/l10n_util.h"
#include "chrome/views/widget/widget.h"

namespace views {

////////////////////////////////////////////////////////////////////////////////
// NativeButtonWin, public:

NativeButtonWin::NativeButtonWin(NativeButtonWrapperListener* listener)
    : NativeControlWin(),
      listener_(listener),
      is_default_(false),
      ignore_minimum_size_(false),
      min_dlu_size_(50, 14) {
  // The min size in DLUs comes from
  // http://msdn.microsoft.com/library/default.asp?url=/library/en-us/dnwue/html/ch14e.asp
}

NativeButtonWin::~NativeButtonWin() {
}

////////////////////////////////////////////////////////////////////////////////
// NativeButtonWin, NativeButtonWrapper implementation:

void NativeButtonWin::SetLabel(const std::wstring& label) {
  label_ = label;

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
  if (l10n_util::AdjustStringForLocaleDirection(label_, &localized_label))
    label_ = localized_label;

  // SetLabel can be called before the view is attached to a view hierarchy, so
  // we check the HWND before attempting to do anything with it.
  if (IsWindow(GetHWND()))
    SetWindowText(GetHWND(), label_.c_str());
}

std::wstring NativeButtonWin::GetLabel() const {
  return label_;
}

void NativeButtonWin::SetFont(const ChromeFont& font) {
  font_ = font;
}

void NativeButtonWin::SetDefaultButton(bool is_default) {
  if (is_default == is_default_)
    return;
  is_default_ = is_default;

  // SetDefaultButton can be called before the view is attached to a view
  // hierarchy, so we check the HWND before attempting to modify its style.
  if (IsWindow(GetHWND())) {
    SendMessage(GetHWND(), BM_SETSTYLE,
                is_default_ ? BS_DEFPUSHBUTTON : BS_PUSHBUTTON, true);
  }
}

bool NativeButtonWin::IsDefaultButton() const {
  return is_default_;
}

void NativeButtonWin::SetMinimumSizeInPlatformUnits(
    const gfx::Size& minimum_size) {
  min_dlu_size_ = minimum_size;
}

void NativeButtonWin::SetIgnoreMinimumSize(bool ignore_minimum_size) {
  ignore_minimum_size_ = ignore_minimum_size;
}

View* NativeButtonWin::GetView() {
  return this;
}

////////////////////////////////////////////////////////////////////////////////
// NativeButtonWin, View overrides:

gfx::Size NativeButtonWin::GetPreferredSize() {
  if (!GetHWND())
    return gfx::Size();

  SIZE sz = {0};
  SendMessage(GetHWND(), BCM_GETIDEALSIZE, 0, reinterpret_cast<LPARAM>(&sz));

  if (!ignore_minimum_size_) {
    if (min_dlu_size_.width()) {
      sz.cx = std::max(static_cast<int>(sz.cx),
                       font_.horizontal_dlus_to_pixels(min_dlu_size_.width()));
    }
    if (min_dlu_size_.height()) {
      sz.cy = std::max(static_cast<int>(sz.cy),
                       font_.vertical_dlus_to_pixels(min_dlu_size_.height()));
    }
  }
  return gfx::Size(sz.cx, sz.cy);
}

////////////////////////////////////////////////////////////////////////////////
// NativeButtonWin, NativeControlWin overrides:

LRESULT NativeButtonWin::ProcessMessage(UINT message,
                                        WPARAM w_param,
                                        LPARAM l_param) {
  if (message == WM_COMMAND && HIWORD(w_param) == BN_CLICKED) {
    listener_->ButtonPressed();
    return 0;
  }
  return NativeControlWin::ProcessMessage(message, w_param, l_param);
}

bool NativeButtonWin::OnKeyDown(int vkey) {
  bool enter_pressed = vkey == VK_RETURN;
  if (enter_pressed)
    listener_->ButtonPressed();
  return enter_pressed;
}

bool NativeButtonWin::NotifyOnKeyDown() const {
  return true;
}

void NativeButtonWin::CreateNativeControl() {
  DWORD flags = WS_CHILD | BS_PUSHBUTTON;
  if (is_default_)
    flags |= BS_DEFPUSHBUTTON;
  HWND control_hwnd = CreateWindowEx(GetAdditionalExStyle(), L"BUTTON", L"",
                                     flags, 0, 0, width(), height(),
                                     GetWidget()->GetNativeView(), NULL, NULL,
                                     NULL);
  NativeControlCreated(control_hwnd);
}

void NativeButtonWin::NativeControlCreated(HWND control_hwnd) {
  NativeControlWin::NativeControlCreated(control_hwnd);

  SendMessage(control_hwnd, WM_SETFONT, reinterpret_cast<WPARAM>(font_.hfont()),
              FALSE);
  SetLabel(GetLabel());
}

////////////////////////////////////////////////////////////////////////////////
// NativeCheckboxWin, public:

NativeCheckboxWin::NativeCheckboxWin(NativeButtonWrapperListener* listener)
    : NativeButtonWin(listener),
      selected_(false) {
}

NativeCheckboxWin::~NativeCheckboxWin() {
}

////////////////////////////////////////////////////////////////////////////////
// NativeCheckboxWin, NativeButtonWrapper implementation:

void NativeCheckboxWin::SetSelected(bool selected) {
  if (selected == selected_)
    return;

  selected_ = selected;
  if (GetHWND()) {
    SendMessage(GetHWND(), BM_SETCHECK, selected ? BST_CHECKED : BST_UNCHECKED,
                0);
  }
}

bool NativeCheckboxWin::IsSelected() const {
  return selected_;
}

void NativeCheckboxWin::SetHighlight(bool highlight) {
  SendMessage(GetHWND(), BM_SETSTATE, highlight, 0);
}

////////////////////////////////////////////////////////////////////////////////
// NativeCheckboxWin, NativeButtonWin overrides:

LRESULT NativeCheckboxWin::ProcessMessage(UINT message,
                                          WPARAM w_param,
                                          LPARAM l_param) {
  if (message == WM_COMMAND && HIWORD(w_param) == BN_CLICKED) {
    SetSelected(!IsSelected());
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
  SetSelected(IsSelected());
}

////////////////////////////////////////////////////////////////////////////////
// NativeRadioButtonWin, public:

NativeRadioButtonWin::NativeRadioButtonWin(
    NativeButtonWrapperListener* listener)
    : NativeCheckboxWin(listener) {
}

NativeRadioButtonWin::~NativeRadioButtonWin() {
}

////////////////////////////////////////////////////////////////////////////////
// NativeRaidoButotnWin, NativeCheckboxWin overrides:

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
    NativeButtonWrapperListener* listener, Type type) {
  switch (type) {
    case TYPE_BUTTON:
      return new NativeButtonWin(listener);
    case TYPE_CHECKBOX:
      return new NativeCheckboxWin(listener);
    case TYPE_RADIOBUTTON:
      return new NativeRadioButtonWin(listener);
  }
  NOTREACHED() << "Invalid button type!";
  return NULL;
}

}  // namespace views
