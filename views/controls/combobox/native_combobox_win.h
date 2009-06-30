// Copyright (c) 2009 The Chromium Authors. All rights reserved. Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#ifndef VIEWS_CONTROLS_COMBOBOX_NATIVE_COMBOBOX_WIN_H_
#define VIEWS_CONTROLS_COMBOBOX_NATIVE_COMBOBOX_WIN_H_

#include "views/controls/combobox/native_combobox_wrapper.h"
#include "views/controls/native_control_win.h"

namespace views {

class NativeComboboxWin : public NativeControlWin,
                          public NativeComboboxWrapper {
 public:
  explicit NativeComboboxWin(Combobox* combobox);
  virtual ~NativeComboboxWin();

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
  // Overridden from NativeControlWin:
  virtual bool ProcessMessage(UINT message,
                              WPARAM w_param,
                              LPARAM l_param,
                              LRESULT* result);
  virtual void CreateNativeControl();
  virtual void NativeControlCreated(HWND native_control);

 private:
  void UpdateFont();

  // The combobox we are bound to.
  Combobox* combobox_;

  // The min width, in pixels, for the text content.
  int content_width_;

  DISALLOW_COPY_AND_ASSIGN(NativeComboboxWin);
};

}  // namespace views

#endif  // VIEWS_CONTROLS_COMBOBOX_NATIVE_COMBOBOX_WIN_H_
