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

#ifndef CHROME_VIEWS_RADIO_BUTTON_H__
#define CHROME_VIEWS_RADIO_BUTTON_H__

#include "chrome/views/checkbox.h"

namespace ChromeViews {

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

  virtual void GetPreferredSize(CSize *out);
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

}
#endif // CHROME_VIEWS_RADIO_BUTTON_H__
