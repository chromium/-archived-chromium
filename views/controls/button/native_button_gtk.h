// Copyright (c) 2009 The Chromium Authors. All rights reserved. Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#ifndef VIEWS_CONTROLS_BUTTON_NATIVE_BUTTON_GTK_H_
#define VIEWS_CONTROLS_BUTTON_NATIVE_BUTTON_GTK_H_

#include "views/controls/button/native_button_wrapper.h"
#include "views/controls/native_control_gtk.h"

namespace views {

// A View that hosts a native GTK button.
class NativeButtonGtk : public NativeControlGtk, public NativeButtonWrapper {
 public:
  explicit NativeButtonGtk(NativeButton* native_button);
  virtual ~NativeButtonGtk();

  // Overridden from NativeButtonWrapper:
  virtual void UpdateLabel();
  virtual void UpdateFont();
  virtual void UpdateEnabled();
  virtual void UpdateDefault();
  virtual View* GetView();
  virtual void SetFocus();
  virtual gfx::NativeView GetTestingHandle() const;

  // Overridden from View:
  virtual gfx::Size GetPreferredSize();

 protected:
  virtual void CreateNativeControl();
  virtual void NativeControlCreated(GtkWidget* widget);

  // Returns true if this button is actually a checkbox or radio button.
  virtual bool IsCheckbox() const { return false; }

 private:
  static void CallClicked(GtkButton* widget, NativeButtonGtk* button);

  // Invoked when the user clicks on the button.
  void OnClicked();

  // The NativeButton we are bound to.
  NativeButton* native_button_;

  DISALLOW_COPY_AND_ASSIGN(NativeButtonGtk);
};

class NativeCheckboxGtk : public NativeButtonGtk {
 public:
  explicit NativeCheckboxGtk(Checkbox* checkbox);

 private:
  virtual void CreateNativeControl();

  // Returns true if this button is actually a checkbox or radio button.
  virtual bool IsCheckbox() const { return true; }
  DISALLOW_COPY_AND_ASSIGN(NativeCheckboxGtk);
};

}  // namespace views

#endif  // #ifndef VIEWS_CONTROLS_BUTTON_NATIVE_BUTTON_GTK_H_
