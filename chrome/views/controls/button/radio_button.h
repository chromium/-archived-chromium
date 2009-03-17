// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_VIEWS_CONTROLS_BUTTON_RADIO_BUTTON_H_
#define CHROME_VIEWS_CONTROLS_BUTTON_RADIO_BUTTON_H_

#include "chrome/views/controls/button/checkbox.h"

namespace views {

////////////////////////////////////////////////////////////////////////////////
//
// A wrapper for windows's native radio button. Radio buttons can be mutually
// exclusive with other radio buttons.
//
////////////////////////////////////////////////////////////////////////////////
class RadioButton : public CheckBox {
 public:
  // The view class name.
  static const char kViewClassName[];

  // Create a radio button with the provided label and group id.
  // The group id is used to identify all the other radio buttons which are in
  // mutual exclusion with this radio button. Note: RadioButton assumes that
  // all views with that group id are RadioButton. It is an error to give
  // that group id to another view subclass which is not a radio button or
  // a radio button subclass.
  RadioButton(const std::wstring& label, int group_id);
  virtual ~RadioButton();

  virtual gfx::Size GetPreferredSize();
  virtual void Layout();

  virtual std::string GetClassName() const;

  // Overridden to properly perform mutual exclusion.
  virtual void SetIsSelected(bool f);

  virtual View* GetSelectedViewForGroup(int group_id);

  // When focusing a RadioButton with Tab/Shift-Tab, only the selected button
  // from the group should be accessible.
  virtual bool IsGroupFocusTraversable() const { return false; }

 protected:
  virtual HWND CreateNativeControl(HWND parent_container);
  virtual LRESULT OnCommand(UINT code, int id, HWND source);

 private:
  // Get the horizontal distance of the start of the text from the left of the
  // control.
  static int GetTextIndent();

  DISALLOW_EVIL_CONSTRUCTORS(RadioButton);
};

}  // namespace views

#endif  // CHROME_VIEWS_CONTROLS_BUTTON_RADIO_BUTTON_H_
