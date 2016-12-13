// Copyright (c) 2009 The Chromium Authors. All rights reserved. Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#include "views/controls/menu/menu_2.h"

#include "base/compiler_specific.h"
#include "views/controls/menu/menu_wrapper.h"

namespace views {

////////////////////////////////////////////////////////////////////////////////
// Menu2Model, public:

// static
bool Menu2Model::GetModelAndIndexForCommandId(int command_id,
                                              Menu2Model** model, int* index) {
  int item_count = (*model)->GetItemCount();
  for (int i = 0; i < item_count; ++i) {
    if ((*model)->GetTypeAt(i) == TYPE_SUBMENU) {
      Menu2Model* submenu_model = (*model)->GetSubmenuModelAt(i);
      if (GetModelAndIndexForCommandId(command_id, &submenu_model, index)) {
        *model = submenu_model;
        return true;
      }
    }
    if ((*model)->GetCommandIdAt(i) == command_id) {
      *index = i;
      return true;
    }
  }
  return false;
}

////////////////////////////////////////////////////////////////////////////////
// Menu2, public:

Menu2::Menu2(Menu2Model* model)
    : model_(model),
      ALLOW_THIS_IN_INITIALIZER_LIST(
          wrapper_(MenuWrapper::CreateWrapper(this))) {
  Rebuild();
}

gfx::NativeMenu Menu2::GetNativeMenu() const {
  return wrapper_->GetNativeMenu();
}

void Menu2::RunMenuAt(const gfx::Point& point, Alignment alignment) {
  wrapper_->RunMenuAt(point, alignment);
}

void Menu2::RunContextMenuAt(const gfx::Point& point) {
  wrapper_->RunMenuAt(point, ALIGN_TOPLEFT);
}

void Menu2::CancelMenu() {
  wrapper_->CancelMenu();
}

void Menu2::Rebuild() {
  wrapper_->Rebuild();
}

void Menu2::UpdateStates() {
  wrapper_->UpdateStates();
}

}  // namespace
