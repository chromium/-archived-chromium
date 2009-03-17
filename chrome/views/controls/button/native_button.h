// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_VIEWS_CONTROLS_BUTTON_NATIVE_BUTTON_H_
#define CHROME_VIEWS_CONTROLS_BUTTON_NATIVE_BUTTON_H_

#include <string>

#include "base/gfx/size.h"
#include "chrome/common/gfx/chrome_font.h"
#include "chrome/views/controls/native_control.h"

namespace views {

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

  virtual gfx::Size GetPreferredSize();

  void SetLabel(const std::wstring& l);
  const std::wstring GetLabel() const;

  std::string GetClassName() const;

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

  // Sets/unsets this button as the default button.  The default button is the
  // one triggered when enter is pressed.  It is displayed with a blue border.
  void SetDefaultButton(bool is_default_button);
  bool IsDefaultButton() const { return is_default_; }

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

  // Sets whether the min size of this button should follow the Windows
  // guidelines.  Default is true.  Set this to false if you want slim buttons.
  void set_enforce_dlu_min_size(bool value) { enforce_dlu_min_size_ = value; }

  // The view class name.
  static const char kViewClassName[];

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

  // Whether the button preferred size should follow the Microsoft layout
  // guidelines.  Default is true.
  bool enforce_dlu_min_size_;

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

  DISALLOW_COPY_AND_ASSIGN(NativeButton);
};

}  // namespace views

#endif  // CHROME_VIEWS_CONTROLS_BUTTON_NATIVE_BUTTON_H_
