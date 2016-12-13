// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/controls/textfield/native_textfield_gtk.h"

namespace views {

////////////////////////////////////////////////////////////////////////////////
// NativeTextfieldGtk, public:

NativeTextfieldGtk::NativeTextfieldGtk(Textfield* textfield)
    : NativeControlGtk() {
}

NativeTextfieldGtk::~NativeTextfieldGtk() {
}

////////////////////////////////////////////////////////////////////////////////
// NativeTextfieldGtk, NativeTextfieldWrapper implementation:

std::wstring NativeTextfieldGtk::GetText() const {
  return std::wstring();
}

void NativeTextfieldGtk::UpdateText() {
}

void NativeTextfieldGtk::AppendText(const std::wstring& text) {
}

std::wstring NativeTextfieldGtk::GetSelectedText() const {
  return std::wstring();
}

void NativeTextfieldGtk::SelectAll() {
}

void NativeTextfieldGtk::ClearSelection() {
}

void NativeTextfieldGtk::UpdateBorder() {
}

void NativeTextfieldGtk::UpdateBackgroundColor() {
}

void NativeTextfieldGtk::UpdateReadOnly() {
}

void NativeTextfieldGtk::UpdateFont() {
}

void NativeTextfieldGtk::UpdateEnabled() {
}

void NativeTextfieldGtk::SetHorizontalMargins(int left, int right) {
}

void NativeTextfieldGtk::SetFocus() {
}

View* NativeTextfieldGtk::GetView() {
  return this;
}

gfx::NativeView NativeTextfieldGtk::GetTestingHandle() const {
  return native_view();
}

////////////////////////////////////////////////////////////////////////////////
// NativeTextfieldGtk, NativeControlGtk overrides:

void NativeTextfieldGtk::CreateNativeControl() {
  // TODO(port): create gtk text field
}

void NativeTextfieldGtk::NativeControlCreated(GtkWidget* widget) {
  NativeControlGtk::NativeControlCreated(widget);
  // TODO(port): post-creation init
}

}  // namespace views
