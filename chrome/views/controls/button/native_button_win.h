// Copyright (c) 2009 The Chromium Authors. All rights reserved. Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#ifndef CHROME_VIEWS_CONTROLS_BUTTON_NATIVE_BUTTON_WIN_H_
#define CHROME_VIEWS_CONTROLS_BUTTON_NATIVE_BUTTON_WIN_H_

#include "chrome/common/gfx/chrome_font.h"
#include "chrome/views/controls/native_control_win.h"
#include "chrome/views/controls/button/native_button_wrapper.h"

namespace views {

// A View that hosts a native Windows button.
class NativeButtonWin : public NativeControlWin,
                        public NativeButtonWrapper {
 public:
  explicit NativeButtonWin(NativeButtonWrapperListener* listener);
  virtual ~NativeButtonWin();

  // Overridden from NativeButtonWrapper:
  virtual void SetLabel(const std::wstring& label);
  virtual std::wstring GetLabel() const;
  virtual void SetFont(const ChromeFont& font);
  virtual void SetDefaultButton(bool is_default);
  virtual bool IsDefaultButton() const;
  virtual void SetMinimumSizeInPlatformUnits(const gfx::Size& minimum_size);
  virtual void SetIgnoreMinimumSize(bool ignore_minimum_size);
  virtual View* GetView();

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

 private:
  // Our listener.
  NativeButtonWrapperListener* listener_;

  // The button label.
  std::wstring label_;

  // True if the button is the default button in its context.
  bool is_default_;

  // The font used to render the button label.
  ChromeFont font_;

  // True if the button should ignore the minimum size for the platform. Default
  // is false.
  bool ignore_minimum_size_;

  // Minimum size, in dlus (see SetMinimumSizeInPlatformUnits).
  gfx::Size min_dlu_size_;

  DISALLOW_COPY_AND_ASSIGN(NativeButtonWin);
};

// A View that hosts a native Windows checkbox.
class NativeCheckboxWin : public NativeButtonWin {
 public:
  explicit NativeCheckboxWin(NativeButtonWrapperListener* listener);
  virtual ~NativeCheckboxWin();

  // Overridden from NativeButtonWrapper:
  virtual void SetSelected(bool selected);
  virtual bool IsSelected() const;
  virtual void SetHighlight(bool highlight);

  // Overridden from NativeControlWin:
  virtual LRESULT ProcessMessage(UINT message,
                                 WPARAM w_param,
                                 LPARAM l_param);

 protected:
  virtual void CreateNativeControl();
  virtual void NativeControlCreated(HWND control_hwnd);

 private:
  // True if this checkbox is checked.
  bool selected_;

  DISALLOW_COPY_AND_ASSIGN(NativeCheckboxWin);
};

// A View that hosts a native Windows radio button.
class NativeRadioButtonWin : public NativeCheckboxWin {
 public:
  explicit NativeRadioButtonWin(NativeButtonWrapperListener* listener);
  virtual ~NativeRadioButtonWin();

 protected:
  // Overridden from NativeCheckboxWin:
  virtual void CreateNativeControl();

 private:
  DISALLOW_COPY_AND_ASSIGN(NativeRadioButtonWin);
};
  
}  // namespace views

#endif  // #ifndef CHROME_VIEWS_CONTROLS_BUTTON_NATIVE_BUTTON_WIN_H_
