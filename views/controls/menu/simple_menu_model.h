// Copyright (c) 2009 The Chromium Authors. All rights reserved. Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#ifndef CONTROLS_MENU_VIEWS_SIMPLE_MENU_MODEL_H_
#define CONTROLS_MENU_VIEWS_SIMPLE_MENU_MODEL_H_

#include <vector>

#include "base/string16.h"
#include "views/controls/menu/menu_2.h"

namespace views {

// A simple Menu2Model implementation with an imperative API for adding menu
// items. This makes it easy to construct fixed menus. Menus populated by
// dynamic data sources may be better off implementing Menu2Model directly.
// The breadth of Menu2Model is not exposed through this API.
class SimpleMenuModel : public Menu2Model {
 public:
  class Delegate {
   public:
    // Methods for determining the state of specific command ids.
    virtual bool IsCommandIdChecked(int command_id) const = 0;
    virtual bool IsCommandIdEnabled(int command_id) const = 0;

    // Gets the accelerator for the specified command id. Returns true if the
    // command id has a valid accelerator, false otherwise.
    virtual bool GetAcceleratorForCommandId(
        int command_id,
        views::Accelerator* accelerator) = 0;

    // Some command ids have labels that change over time.
    virtual bool IsLabelForCommandIdDynamic(int command_id) const {
      return false;
    }
    virtual string16 GetLabelForCommandId(int command_id) const {
      return string16();
    }

    // Notifies the delegate that the item with the specified command id was
    // visually highlighted within the menu.
    virtual void CommandIdHighlighted(int command_id) {}

    // Performs the action associated with the specified command id.
    virtual void ExecuteCommand(int command_id) = 0;
  };

  // The Delegate can be NULL, though if it is items can't be checked or
  // disabled.
  explicit SimpleMenuModel(Delegate* delegate);
  virtual ~SimpleMenuModel();

  // Methods for adding items to the model.
  void AddItem(int command_id, const string16& label);
  void AddItemWithStringId(int command_id, int string_id);
  void AddSeparator();
  void AddCheckItem(int command_id, const string16& label);
  void AddCheckItemWithStringId(int command_id, int string_id);
  void AddRadioItem(int command_id, const string16& label, int group_id);
  void AddRadioItemWithStringId(int command_id, int string_id, int group_id);
  void AddSubMenu(const string16& label, Menu2Model* model);
  void AddSubMenuWithStringId(int string_id, Menu2Model* model);

  // Overridden from Menu2Model:
  virtual bool HasIcons() const;
  virtual int GetItemCount() const;
  virtual ItemType GetTypeAt(int index) const;
  virtual int GetCommandIdAt(int index) const;
  virtual string16 GetLabelAt(int index) const;
  virtual bool IsLabelDynamicAt(int index) const;
  virtual bool GetAcceleratorAt(int index,
                                views::Accelerator* accelerator) const;
  virtual bool IsItemCheckedAt(int index) const;
  virtual int GetGroupIdAt(int index) const;
  virtual bool GetIconAt(int index, SkBitmap* icon) const;
  virtual bool IsEnabledAt(int index) const;
  virtual void HighlightChangedTo(int index);
  virtual void ActivatedAt(int index);
  virtual Menu2Model* GetSubmenuModelAt(int index) const;

 protected:
  // Some variants of this model (SystemMenuModel) relies on items to be
  // inserted backwards. This is counter-intuitive for the API, so rather than
  // forcing customers to insert things backwards, we return the indices
  // backwards instead. That's what this method is for. By default, it just
  // returns what it's passed.
  virtual int FlipIndex(int index) const { return index; }

 private:
  struct Item {
    int command_id;
    string16 label;
    ItemType type;
    int group_id;
    Menu2Model* submenu;
  };
  std::vector<Item> items_;

  Delegate* delegate_;

  DISALLOW_COPY_AND_ASSIGN(SimpleMenuModel);
};

}  // namespace views

#endif  // CONTROLS_MENU_VIEWS_SIMPLE_MENU_MODEL_H_
