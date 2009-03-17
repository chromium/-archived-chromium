// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_VIEWS_CONTROLS_BUTTON_CHECKBOX_H_
#define CHROME_VIEWS_CONTROLS_BUTTON_CHECKBOX_H_

#include "base/gfx/rect.h"
#include "chrome/views/controls/button/native_button.h"

namespace views {

class Label;

////////////////////////////////////////////////////////////////////////////////
//
// CheckBox implements a check box button. It uses the standard windows control
// for the check item but not for the label. We have to do this because windows
// always wants to draw a background under the label. I tried to intercept
// WM_CTLCOLORSTATIC and return a NULL_BRUSH and setting the BkMode to
// transparent as well as other things. The background was always drawn as solid
// black.
//
// The label is implemented with a views::Label
//
////////////////////////////////////////////////////////////////////////////////
class CheckBox : public NativeButton {
 public:
  static const char kViewClassName[];
  static const int kFocusPaddingHorizontal;
  static const int kFocusPaddingVertical;

  explicit CheckBox(const std::wstring& label);
  virtual ~CheckBox();

  // Allows the label to wrap across multiple lines if |multi_line| is true.
  // If false, the text is cropped.
  void SetMultiLine(bool multi_line);

  // Returns the x position of the text. This can also be used to indent
  // subsequent dependent controls.
  static int GetTextIndent();

  virtual void SetIsSelected(bool f);
  bool IsSelected() const;

  virtual std::string GetClassName() const;

  virtual gfx::Size GetPreferredSize();
  virtual void Layout();

  virtual bool OnMousePressed(const MouseEvent& event);
  virtual bool OnMouseDragged(const MouseEvent& event);
  virtual void OnMouseReleased(const MouseEvent& event, bool canceled);
  virtual void OnMouseEntered(const MouseEvent& event);
  virtual void OnMouseMoved(const MouseEvent& event);
  virtual void OnMouseExited(const MouseEvent& event);

  virtual void Paint(ChromeCanvas* canvas);

  // Overriden to forward to the label too.
  virtual void SetEnabled(bool enabled);

 protected:

  virtual HWND CreateNativeControl(HWND parent_container);
  virtual void ConfigureNativeButton(HWND hwnd);
  virtual LRESULT OnCommand(UINT code, int id, HWND source);

  Label* label_;
 private:

  void HighlightButton(bool f);
  bool LabelHitTest(const MouseEvent& event);
  void ComputeTextRect(gfx::Rect* out);

  bool is_selected_;


  DISALLOW_EVIL_CONSTRUCTORS(CheckBox);
};

}  // namespace views

#endif  // CHROME_VIEWS_CONTROLS_BUTTON_CHECKBOX_H_
