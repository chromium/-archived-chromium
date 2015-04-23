// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VIEWS_CONTROLS_COMBOBOX_COMBOBOX_H_
#define VIEWS_CONTROLS_COMBOBOX_COMBOBOX_H_

#include "views/view.h"

namespace views {

class NativeComboboxWrapper;

// A non-editable combo-box.
class Combobox : public View {
 public:
  // The combobox's class name.
  static const char kViewClassName[];

  class Model {
   public:
    // Return the number of items in the combo box.
    virtual int GetItemCount(Combobox* source) = 0;

    // Return the string that should be used to represent a given item.
    virtual std::wstring GetItemAt(Combobox* source, int index) = 0;
  };

  class Listener {
   public:
    // This is invoked once the selected item changed.
    virtual void ItemChanged(Combobox* combo_box,
                             int prev_index,
                             int new_index) = 0;
  };

  // |model| is not owned by the combo box.
  explicit Combobox(Model* model);
  virtual ~Combobox();

  // Register |listener| for item change events.
  void set_listener(Listener* listener) {
    listener_ = listener;
  }

  // Inform the combo box that its model changed.
  void ModelChanged();

  // Gets/Sets the selected item.
  int selected_item() const { return selected_item_; };
  void SetSelectedItem(int index);

  // Called when the combo box's selection is changed by the user.
  void SelectionChanged();

  // Accessor for |model_|.
  Model* model() const { return model_; }

  // Overridden from View:
  virtual gfx::Size GetPreferredSize();
  virtual void Layout();
  virtual void SetEnabled(bool enabled);
  virtual bool SkipDefaultKeyEventProcessing(const KeyEvent& e);

 protected:
  virtual void Focus();
  virtual void ViewHierarchyChanged(bool is_add, View* parent,
                                    View* child);
  virtual std::string GetClassName() const;

  // The object that actually implements the native combobox.
  NativeComboboxWrapper* native_wrapper_;

 private:
  // Our model.
  Model* model_;

  // Item change listener.
  Listener* listener_;

  // The current selection.
  int selected_item_;

  DISALLOW_COPY_AND_ASSIGN(Combobox);
};

}  // namespace views

#endif  // VIEWS_CONTROLS_COMBOBOX_COMBOBOX_H_
