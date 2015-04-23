// Copyright (c) 2009 The Chromium Authors. All rights reserved. Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#ifndef VIEWS_CONTROLS_COMBOBOX_NATIVE_COMBOBOX_GTK_H_
#define VIEWS_CONTROLS_COMBOBOX_NATIVE_COMBOBOX_GTK_H_

#include "views/controls/combobox/native_combobox_wrapper.h"
#include "views/controls/native_control_gtk.h"

namespace views {

class NativeComboboxGtk : public NativeControlGtk,
                          public NativeComboboxWrapper {
 public:
  explicit NativeComboboxGtk(Combobox* combobox);
  virtual ~NativeComboboxGtk();

  // Overridden from NativeComboboxWrapper:
  virtual void UpdateFromModel();
  virtual void UpdateSelectedItem();
  virtual void UpdateEnabled();
  virtual int GetSelectedItem() const;
  virtual bool IsDropdownOpen() const;
  virtual gfx::Size GetPreferredSize() const;
  virtual View* GetView();
  virtual void SetFocus();
  virtual gfx::NativeView GetTestingHandle() const;

 protected:
  // Overridden from NativeControlGtk:
  virtual void CreateNativeControl();
  virtual void NativeControlCreated(GtkWidget* widget);

 private:
  // The combobox we are bound to.
  Combobox* combobox_;

  DISALLOW_COPY_AND_ASSIGN(NativeComboboxGtk);
};

}  // namespace views

#endif  // VIEWS_CONTROLS_COMBOBOX_NATIVE_COMBOBOX_GTK_H_
