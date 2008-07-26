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

#ifndef CHROME_VIEWS_CHECKBOX_H__
#define CHROME_VIEWS_CHECKBOX_H__

#include "base/gfx/rect.h"
#include "chrome/views/native_button.h"

namespace ChromeViews {

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
// The label is implemented with a ChromeViews::Label
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

  virtual void GetPreferredSize(CSize *out);
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
}

#endif  // CHROME_VIEWS_NATIVE_CHECKBOX_H__
