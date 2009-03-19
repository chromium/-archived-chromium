// Copyright (c) 2009 The Chromium Authors. All rights reserved. Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#include "chrome/views/controls/button/checkbox2.h"

#include "chrome/views/controls/label.h"

namespace views {

// static
const char Checkbox2::kViewClassName[] = "chrome/views/Checkbox";

////////////////////////////////////////////////////////////////////////////////
// Checkbox2, public:

Checkbox2::Checkbox2() : NativeButton2(NULL), checked_(false) {
  CreateLabel(std::wstring());
}

Checkbox2::Checkbox2(ButtonListener* listener)
    : NativeButton2(listener),
      checked_(false) {
  CreateLabel(std::wstring());
}

Checkbox2::Checkbox2(ButtonListener* listener, const std::wstring& label)
    : NativeButton2(listener, label),
      checked_(false) {
  CreateLabel(label);
}

Checkbox2::~Checkbox2() {
}

void Checkbox2::SetMultiline(bool multiline) {
  label_->SetMultiLine(multiline);
}

void Checkbox2::SetChecked(bool checked) {
  if (checked_ == checked)
    return;
  checked_ = checked;
  native_wrapper_->UpdateChecked();
}

////////////////////////////////////////////////////////////////////////////////
// Checkbox2, View overrides:

gfx::Size Checkbox2::GetPreferredSize() {
  return gfx::Size(120, 30);
}

void Checkbox2::Layout() {
}

std::string Checkbox2::GetClassName() const {
  return kViewClassName;
}

////////////////////////////////////////////////////////////////////////////////
// Checkbox2, NativeButton2 overrides:

void Checkbox2::CreateWrapper() {
  native_wrapper_ = NativeButtonWrapper::CreateCheckboxWrapper(this);
  native_wrapper_->UpdateLabel();
  native_wrapper_->UpdateChecked();
}

void Checkbox2::InitBorder() {
  // No border, so we do nothing.
}

////////////////////////////////////////////////////////////////////////////////
// Checkbox2, private:

void Checkbox2::CreateLabel(const std::wstring& label_text) {
  set_minimum_size(gfx::Size(0, 0));
  label_ = new Label(label_text);
  label_->SetHorizontalAlignment(Label::ALIGN_LEFT);
  AddChildView(label_);
}

}  // namespace views
