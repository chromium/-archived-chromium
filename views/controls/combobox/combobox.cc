// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/controls/combobox/combobox.h"

#include "base/keyboard_codes.h"
#include "base/logging.h"
#include "views/controls/combobox/native_combobox_wrapper.h"

namespace views {

// static
const char Combobox::kViewClassName[] = "views/Combobox";

////////////////////////////////////////////////////////////////////////////////
// Combobox, public:

Combobox::Combobox(Model* model)
    : native_wrapper_(NULL),
      model_(model),
      listener_(NULL),
      selected_item_(0) {
  SetFocusable(true);
}

Combobox::~Combobox() {
}

void Combobox::ModelChanged() {
  selected_item_ = std::min(0, model_->GetItemCount(this));
  if (native_wrapper_)
    native_wrapper_->UpdateFromModel();
}

void Combobox::SetSelectedItem(int index) {
  selected_item_ = index;
  if (native_wrapper_)
    native_wrapper_->UpdateSelectedItem();
}

void Combobox::SelectionChanged() {
  int prev_selected_item = selected_item_;
  selected_item_ = native_wrapper_->GetSelectedItem();
  if (listener_)
    listener_->ItemChanged(this, prev_selected_item, selected_item_);
}

////////////////////////////////////////////////////////////////////////////////
// Combobox, View overrides:

gfx::Size Combobox::GetPreferredSize() {
  if (native_wrapper_)
    return native_wrapper_->GetPreferredSize();
  return gfx::Size();
}

void Combobox::Layout() {
  if (native_wrapper_) {
    native_wrapper_->GetView()->SetBounds(0, 0, width(), height());
    native_wrapper_->GetView()->Layout();
  }
}

void Combobox::SetEnabled(bool flag) {
  View::SetEnabled(flag);
  if (native_wrapper_)
    native_wrapper_->UpdateEnabled();
}

// VK_ESCAPE should be handled by this view when the drop down list is active.
// In other words, the list should be closed instead of the dialog.
bool Combobox::SkipDefaultKeyEventProcessing(const KeyEvent& e) {
  if (e.GetCharacter() != base::VKEY_ESCAPE ||
      e.IsShiftDown() || e.IsControlDown() || e.IsAltDown()) {
    return false;
  }
  return native_wrapper_ && native_wrapper_->IsDropdownOpen();
}

void Combobox::Focus() {
  // Forward the focus to the wrapper.
  if (native_wrapper_)
    native_wrapper_->SetFocus();
  else
    View::Focus();  // Will focus the RootView window (so we still get
                    // keyboard messages).
}

void Combobox::ViewHierarchyChanged(bool is_add, View* parent,
                                    View* child) {
  if (is_add && !native_wrapper_ && GetWidget()) {
    native_wrapper_ = NativeComboboxWrapper::CreateWrapper(this);
    native_wrapper_->UpdateFromModel();
    native_wrapper_->UpdateEnabled();
    AddChildView(native_wrapper_->GetView());
  }
}

std::string Combobox::GetClassName() const {
  return kViewClassName;
}

}  // namespace views
