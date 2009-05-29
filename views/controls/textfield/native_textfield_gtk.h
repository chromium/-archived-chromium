// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VIEWS_CONTROLS_TEXTFIELD_NATIVE_TEXTFIELD_GTK_H_
#define VIEWS_CONTROLS_TEXTFIELD_NATIVE_TEXTFIELD_GTK_H_

#include "views/controls/native_control_gtk.h"
#include "views/controls/textfield/native_textfield_wrapper.h"

namespace views {

class NativeTextfieldGtk : public NativeControlGtk,
                           public NativeTextfieldWrapper {
 public:
  explicit NativeTextfieldGtk(Textfield* parent);
  ~NativeTextfieldGtk();

  // Overridden from NativeTextfieldWrapper:
  virtual std::wstring GetText() const;
  virtual void UpdateText();
  virtual void AppendText(const std::wstring& text);
  virtual std::wstring GetSelectedText() const;
  virtual void SelectAll();
  virtual void ClearSelection();
  virtual void UpdateBorder();
  virtual void UpdateBackgroundColor();
  virtual void UpdateReadOnly();
  virtual void UpdateFont();
  virtual void UpdateEnabled();
  virtual void SetHorizontalMargins(int left, int right);
  virtual void SetFocus();
  virtual View* GetView();
  virtual gfx::NativeView GetTestingHandle() const;

  // Overridden from NativeControlGtk:
  virtual void CreateNativeControl();
  virtual void NativeControlCreated(GtkWidget* widget);

 private:
  DISALLOW_COPY_AND_ASSIGN(NativeTextfieldGtk);
};

}  // namespace views

#endif  // VIEWS_CONTROLS_TEXTFIELD_NATIVE_TEXTFIELD_GTK_H_
