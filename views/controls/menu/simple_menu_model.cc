// Copyright (c) 2009 The Chromium Authors. All rights reserved. Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#include "views/controls/menu/simple_menu_model.h"

#include "app/l10n_util.h"

namespace views {

////////////////////////////////////////////////////////////////////////////////
// SimpleMenuModel, public:

SimpleMenuModel::SimpleMenuModel(Delegate* delegate) : delegate_(delegate) {
}

SimpleMenuModel::~SimpleMenuModel() {
}

void SimpleMenuModel::AddItem(int command_id, const string16& label) {
  Item item = { command_id, label, TYPE_COMMAND, -1, NULL };
  items_.push_back(item);
}

void SimpleMenuModel::AddItemWithStringId(int command_id, int string_id) {
  AddItem(command_id, l10n_util::GetStringUTF16(string_id));
}

void SimpleMenuModel::AddSeparator() {
  Item item = { -1, string16(), TYPE_SEPARATOR, -1, NULL };
  items_.push_back(item);
}

void SimpleMenuModel::AddCheckItem(int command_id, const string16& label) {
  Item item = { command_id, label, TYPE_CHECK, -1, NULL };
  items_.push_back(item);
}

void SimpleMenuModel::AddCheckItemWithStringId(int command_id, int string_id) {
  AddCheckItem(command_id, l10n_util::GetStringUTF16(string_id));
}

void SimpleMenuModel::AddRadioItem(int command_id, const string16& label,
                                   int group_id) {
  Item item = { command_id, label, TYPE_RADIO, group_id, NULL };
  items_.push_back(item);
}

void SimpleMenuModel::AddRadioItemWithStringId(int command_id, int string_id,
                                               int group_id) {
  AddRadioItem(command_id, l10n_util::GetStringUTF16(string_id), group_id);
}

void SimpleMenuModel::AddSubMenu(const string16& label, Menu2Model* model) {
  Item item = { -1, label, TYPE_SUBMENU, -1, model };
  items_.push_back(item);
}

void SimpleMenuModel::AddSubMenuWithStringId(int string_id, Menu2Model* model) {
  AddSubMenu(l10n_util::GetStringUTF16(string_id), model);
}

////////////////////////////////////////////////////////////////////////////////
// SimpleMenuModel, Menu2Model implementation:

bool SimpleMenuModel::HasIcons() const {
  return false;
}

int SimpleMenuModel::GetItemCount() const {
  return static_cast<int>(items_.size());
}

Menu2Model::ItemType SimpleMenuModel::GetTypeAt(int index) const {
  return items_.at(FlipIndex(index)).type;
}

int SimpleMenuModel::GetCommandIdAt(int index) const {
  return items_.at(FlipIndex(index)).command_id;
}

string16 SimpleMenuModel::GetLabelAt(int index) const {
  if (IsLabelDynamicAt(index))
    return delegate_->GetLabelForCommandId(GetCommandIdAt(index));
  return items_.at(FlipIndex(index)).label;
}

bool SimpleMenuModel::IsLabelDynamicAt(int index) const {
  if (delegate_)
    return delegate_->IsLabelForCommandIdDynamic(GetCommandIdAt(index));
  return false;
}

bool SimpleMenuModel::GetAcceleratorAt(int index,
                                       views::Accelerator* accelerator) const {
  if (delegate_) {
    return delegate_->GetAcceleratorForCommandId(GetCommandIdAt(index),
                                                 accelerator);
  }
  return false;
}

bool SimpleMenuModel::IsItemCheckedAt(int index) const {
  if (!delegate_)
    return false;
  return delegate_->IsCommandIdChecked(GetCommandIdAt(index));
}

int SimpleMenuModel::GetGroupIdAt(int index) const {
  return items_.at(FlipIndex(index)).group_id;
}

bool SimpleMenuModel::GetIconAt(int index, SkBitmap* icon) const {
  return false;
}

bool SimpleMenuModel::IsEnabledAt(int index) const {
  int command_id = GetCommandIdAt(index);
  // Submenus have a command id of -1, they should always be enabled.
  if (!delegate_ || command_id == -1)
    return true;
  return delegate_->IsCommandIdEnabled(command_id);
}

void SimpleMenuModel::HighlightChangedTo(int index) {
  if (delegate_)
    delegate_->CommandIdHighlighted(GetCommandIdAt(index));
}

void SimpleMenuModel::ActivatedAt(int index) {
  if (delegate_)
    delegate_->ExecuteCommand(GetCommandIdAt(index));
}

Menu2Model* SimpleMenuModel::GetSubmenuModelAt(int index) const {
  return items_.at(FlipIndex(index)).submenu;
}

}  // namespace views
