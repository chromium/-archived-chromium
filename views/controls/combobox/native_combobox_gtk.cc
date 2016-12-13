// Copyright (c) 2009 The Chromium Authors. All rights reserved. Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#include "views/controls/combobox/native_combobox_gtk.h"

#include "base/logging.h"
#include "views/controls/combobox/combobox.h"

namespace views {

////////////////////////////////////////////////////////////////////////////////
// NativeComboboxGtk, public:

NativeComboboxGtk::NativeComboboxGtk(Combobox* combobox)
    : combobox_(combobox) {
}

NativeComboboxGtk::~NativeComboboxGtk() {
}

////////////////////////////////////////////////////////////////////////////////
// NativeComboboxGtk, NativeComboboxWrapper implementation:

void NativeComboboxGtk::UpdateFromModel() {
  NOTIMPLEMENTED();
}

void NativeComboboxGtk::UpdateSelectedItem() {
  NOTIMPLEMENTED();
}

void NativeComboboxGtk::UpdateEnabled() {
  NOTIMPLEMENTED();
}

int NativeComboboxGtk::GetSelectedItem() const {
  NOTIMPLEMENTED();
  return 0;
}

bool NativeComboboxGtk::IsDropdownOpen() const {
  NOTIMPLEMENTED();
  return false;
}

gfx::Size NativeComboboxGtk::GetPreferredSize() const {
  NOTIMPLEMENTED();
  return gfx::Size();
}

View* NativeComboboxGtk::GetView() {
  return this;
}

void NativeComboboxGtk::SetFocus() {
  Focus();
}

gfx::NativeView NativeComboboxGtk::GetTestingHandle() const {
  return native_view();
}

////////////////////////////////////////////////////////////////////////////////
// NativeComboboxGtk, NativeControlGtk overrides:

void NativeComboboxGtk::CreateNativeControl() {
}

void NativeComboboxGtk::NativeControlCreated(GtkWidget* native_control) {
  NativeControlGtk::NativeControlCreated(native_control);
}

////////////////////////////////////////////////////////////////////////////////
// NativeComboboxWrapper, public:

// static
NativeComboboxWrapper* NativeComboboxWrapper::CreateWrapper(
    Combobox* combobox) {
  return new NativeComboboxGtk(combobox);
}

}  // namespace views
