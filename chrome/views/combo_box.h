// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_VIEWS_COMBO_BOX_H__
#define CHROME_VIEWS_COMBO_BOX_H__

#include "chrome/views/native_control.h"

namespace views {
////////////////////////////////////////////////////////////////////////////////
//
// ComboBox is a basic non editable combo box. It is initialized from a simple
// model.
//
////////////////////////////////////////////////////////////////////////////////
class ComboBox : public NativeControl {
 public:
  class Model {
   public:
    // Return the number of items in the combo box.
    virtual int GetItemCount(ComboBox* source) = 0;

    // Return the string that should be used to represent a given item.
    virtual std::wstring GetItemAt(ComboBox* source, int index) = 0;
  };

  class Listener {
   public:
    // This is invoked once the selected item changed.
    virtual void ItemChanged(ComboBox* combo_box,
                             int prev_index,
                             int new_index) = 0;
  };

  // |model is not owned by the combo box.
  explicit ComboBox(Model* model);
  virtual ~ComboBox();

  // Register |listener| for item change events.
  void SetListener(Listener* listener);

  // Overriden from View.
  virtual gfx::Size GetPreferredSize();

  // Overriden from NativeControl
  virtual HWND CreateNativeControl(HWND parent_container);
  virtual LRESULT OnCommand(UINT code, int id, HWND source);
  virtual LRESULT OnNotify(int w_param, LPNMHDR l_param);

  // Inform the combo box that its model changed.
  void ModelChanged();

  // Set / Get the selected item.
  void SetSelectedItem(int index);
  int GetSelectedItem();

 private:
  // Update a combo box from our model.
  void UpdateComboBoxFromModel(HWND hwnd);

  // Our model.
  Model* model_;

  // The current selection.
  int selected_item_;

  // Item change listener.
  Listener* listener_;

  // The min width, in pixels, for the text content.
  int content_width_;

  DISALLOW_EVIL_CONSTRUCTORS(ComboBox);
};

}  // namespace views

#endif  // CHROME_VIEWS_COMBO_BOX_H__
