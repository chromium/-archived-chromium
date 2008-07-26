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

#ifndef CHROME_VIEWS_NATIVE_BUTTON_H__
#define CHROME_VIEWS_NATIVE_BUTTON_H__

#include <string>

#include "base/gfx/size.h"
#include "chrome/common/gfx/chrome_font.h"
#include "chrome/views/native_control.h"

namespace ChromeViews {


class HWNDView;

////////////////////////////////////////////////////////////////////////////////
//
// NativeButton is a wrapper for a windows native button
// TODO(beng): (Cleanup) this should derive also from some button-like base to
//             get all the listenery-stuff for free and to work with
//             AddManagedButton.
//
////////////////////////////////////////////////////////////////////////////////
class NativeButton : public NativeControl {
 public:
  explicit NativeButton(const std::wstring& label);
  NativeButton(const std::wstring& label, bool is_default);
  virtual ~NativeButton();

  virtual void GetPreferredSize(CSize *out);

  void SetLabel(const std::wstring& l);
  const std::wstring GetLabel() const;

  class Listener {
   public:
    //
    // This is invoked once the button is released.
    virtual void ButtonPressed(NativeButton* sender) = 0;
  };

  //
  // The the listener, the object that receives a notification when this
  // button is pressed.
  void SetListener(Listener* listener);

  // Set the font used by this button.
  void SetFont(const ChromeFont& font) { font_ = font; }

  // Adds some internal padding to the button.  The |size| specified is applied
  // on both sides of the button for each directions
  void SetPadding(CSize size);

  virtual LRESULT OnNotify(int w_param, LPNMHDR l_param);
  virtual LRESULT OnCommand(UINT code, int id, HWND source);

  // Invoked when the accelerator associated with the button is pressed.
  virtual bool AcceleratorPressed(const Accelerator& accelerator);

  // Sets the minimum size of the button from the specified size (in dialog
  // units). If the width/height is non-zero, the preferred size of the button
  // is max(preferred size of the content + padding, dlus converted to pixels).
  //
  // The default is 50, 14.
  void SetMinSizeFromDLUs(const gfx::Size& dlu_size) {
    min_dlu_size_ = dlu_size;
  }

  // Returns the MSAA role of the current view. The role is what assistive
  // technologies (ATs) use to determine what behavior to expect from a given
  // control.
  bool GetAccessibleRole(VARIANT* role);

  // Returns a brief, identifying string, containing a unique, readable name.
  bool GetAccessibleName(std::wstring* name);

  // Assigns an accessible string name.
  void SetAccessibleName(const std::wstring& name);

 protected:

  virtual HWND CreateNativeControl(HWND parent_container);

  // Sub-classes can call this method to cause the native button to reflect
  // the current state
  virtual void UpdateNativeButton();

  // Sub-classes must override this method to properly configure the native
  // button given the current state
  virtual void ConfigureNativeButton(HWND hwnd);

  // Overridden from NativeControl so we can activate the button when Enter is
  // pressed.
  virtual bool NotifyOnKeyDown() const;
  virtual bool OnKeyDown(int virtual_key_code);

 private:
  NativeButton() {}

  // Initializes the button. If |is_default| is true, this appears like the
  // "default" button, and will register Enter as its keyboard accelerator.
  void Init(const std::wstring& label, bool is_default);

  void Clicked();

  std::wstring label_;
  ChromeFont font_;
  Listener* listener_;

  CSize padding_;

  // True if the button should be rendered to appear like the "default" button
  // in the containing dialog box. Default buttons register Enter as their
  // accelerator.
  bool is_default_;

  // Minimum size, in dlus (see SetMinSizeFromDLUs).
  gfx::Size min_dlu_size_;

  // Storage of strings needed for accessibility.
  std::wstring accessible_name_;

  DISALLOW_EVIL_CONSTRUCTORS(NativeButton);
};

}

#endif  // CHROME_VIEWS_NATIVE_BUTTON_H__
