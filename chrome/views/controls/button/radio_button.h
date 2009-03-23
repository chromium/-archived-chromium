// Copyright (c) 2009 The Chromium Authors. All rights reserved. Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#ifndef CHROME_VIEWS_CONTROLS_BUTTON_RADIO_BUTTON_H_
#define CHROME_VIEWS_CONTROLS_BUTTON_RADIO_BUTTON_H_

#include "chrome/views/controls/button/checkbox.h"

namespace views {

// A Checkbox subclass representing a radio button.
class RadioButton : public Checkbox {
 public:
  // The button's class name.
  static const char kViewClassName[];

  RadioButton();
  RadioButton(const std::wstring& label);
  RadioButton(const std::wstring& label, int group_id);
  virtual ~RadioButton();

  // Overridden from Checkbox:
  virtual void SetChecked(bool checked);

  // Overridden from View:
  virtual View* GetSelectedViewForGroup(int group_id);
  virtual bool IsGroupFocusTraversable() const;

 protected:
  virtual std::string GetClassName() const;

  // Overridden from NativeButton:
  virtual void CreateWrapper();

 private:
  DISALLOW_COPY_AND_ASSIGN(RadioButton);
};

}  // namespace views

#endif  // #ifndef CHROME_VIEWS_CONTROLS_BUTTON_RADIO_BUTTON_H_
