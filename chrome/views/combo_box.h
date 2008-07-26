// Copyright 2008, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#ifndef CHROME_VIEWS_COMBO_BOX_H__
#define CHROME_VIEWS_COMBO_BOX_H__

#include "chrome/views/native_control.h"

namespace ChromeViews {
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
  virtual void GetPreferredSize(CSize* out);

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
}

#endif  // CHROME_VIEWS_COMBO_BOX_H__
