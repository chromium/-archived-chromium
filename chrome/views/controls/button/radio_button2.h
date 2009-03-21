// Copyright (c) 2009 The Chromium Authors. All rights reserved. Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#ifndef CHROME_VIEWS_CONTROLS_BUTTON_RADIO_BUTTON2_H_
#define CHROME_VIEWS_CONTROLS_BUTTON_RADIO_BUTTON2_H_

#include "chrome/views/controls/button/checkbox2.h"

namespace views {

// A Checkbox subclass representing a radio button.
class RadioButton2 : public Checkbox2 {
 public:
  // The button's class name.
  static const char kViewClassName[];

  RadioButton2();
  explicit RadioButton2(ButtonListener* listener);
  RadioButton2(ButtonListener* listener, const std::wstring& label);
  RadioButton2(ButtonListener* listener,
               const std::wstring& label,
               int group_id);
  virtual ~RadioButton2();

  // Overridden from Checkbox:
  virtual void SetChecked(bool checked);

  // Overridden from View:
  virtual View* GetSelectedViewForGroup(int group_id);
  virtual bool IsGroupFocusTraversable() const;

 protected:
  virtual std::string GetClassName() const;

  // Overridden from NativeButton2:
  virtual void CreateWrapper();

 private:
  DISALLOW_COPY_AND_ASSIGN(RadioButton2);
};

}  // namespace views

#endif  // #ifndef CHROME_VIEWS_CONTROLS_BUTTON_CHECKBOX2_H_
