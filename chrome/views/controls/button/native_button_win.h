// Copyright (c) 2009 The Chromium Authors. All rights reserved. Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#ifndef CHROME_VIEWS_CONTROLS_BUTTON_NATIVE_BUTTON_WIN_H_
#define CHROME_VIEWS_CONTROLS_BUTTON_NATIVE_BUTTON_WIN_H_

#include "chrome/views/controls/native_control_win.h"
#include "chrome/views/controls/button/native_button_wrapper.h"

namespace views {

// A View that hosts a native Windows button.
class NativeButtonWin : public NativeControlWin,
                        public NativeButtonWrapper {
 public:
  explicit NativeButtonWin(NativeButton* native_button);
  virtual ~NativeButtonWin();

  // Overridden from NativeButtonWrapper:
  virtual void UpdateLabel();
  virtual void UpdateFont();
  virtual void UpdateDefault();
  virtual View* GetView();
  virtual void SetFocus();

  // Overridden from View:
  virtual gfx::Size GetPreferredSize();

  // Overridden from NativeControlWin:
  virtual LRESULT ProcessMessage(UINT message,
                                 WPARAM w_param,
                                 LPARAM l_param);
  virtual bool OnKeyDown(int vkey);

 protected:
  virtual bool NotifyOnKeyDown() const;
  virtual void CreateNativeControl();
  virtual void NativeControlCreated(HWND control_hwnd);

  // Returns true if this button is actually a checkbox or radio button.
  virtual bool IsCheckbox() const { return false; }

 private:
  // The NativeButton we are bound to.
  NativeButton* native_button_;

  DISALLOW_COPY_AND_ASSIGN(NativeButtonWin);
};

// A View that hosts a native Windows checkbox.
class NativeCheckboxWin : public NativeButtonWin {
 public:
  explicit NativeCheckboxWin(Checkbox* native_button);
  virtual ~NativeCheckboxWin();

  // Overridden from View:
  virtual gfx::Size GetPreferredSize();

  // Overridden from NativeButtonWrapper:
  virtual void UpdateChecked();
  virtual void SetPushed(bool pushed);

  // Overridden from NativeControlWin:
  virtual void SetFocus();
  virtual LRESULT ProcessMessage(UINT message,
                                 WPARAM w_param,
                                 LPARAM l_param);

 protected:
  virtual void CreateNativeControl();
  virtual void NativeControlCreated(HWND control_hwnd);
  virtual bool IsCheckbox() const { return true; }

 private:
  // The Checkbox we are bound to.
  Checkbox* checkbox_;

  DISALLOW_COPY_AND_ASSIGN(NativeCheckboxWin);
};

// A View that hosts a native Windows radio button.
class NativeRadioButtonWin : public NativeCheckboxWin {
 public:
  explicit NativeRadioButtonWin(RadioButton* radio_button);
  virtual ~NativeRadioButtonWin();

 protected:
  // Overridden from NativeCheckboxWin:
  virtual void CreateNativeControl();

 private:
  DISALLOW_COPY_AND_ASSIGN(NativeRadioButtonWin);
};

}  // namespace views

#endif  // #ifndef CHROME_VIEWS_CONTROLS_BUTTON_NATIVE_BUTTON_WIN_H_
