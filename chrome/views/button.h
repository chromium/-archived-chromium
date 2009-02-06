// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_VIEWS_BUTTON_H_
#define CHROME_VIEWS_BUTTON_H_

#include "chrome/views/base_button.h"
#include "skia/include/SkBitmap.h"

namespace views {

////////////////////////////////////////////////////////////////////////////////
//
// Button
//
// A simple button class
//
////////////////////////////////////////////////////////////////////////////////
class Button : public BaseButton {
 public:
  //
  // Create a Button
  Button();
  virtual ~Button();

  //
  // Set the image the button should use for the provided state.
  virtual void SetImage(ButtonState aState, SkBitmap* anImage);

  enum HorizontalAlignment { ALIGN_LEFT = 0,
                             ALIGN_CENTER,
                             ALIGN_RIGHT, };

  enum VerticalAlignment {ALIGN_TOP = 0,
                          ALIGN_MIDDLE,
                          ALIGN_BOTTOM };

  void SetImageAlignment(HorizontalAlignment h_align,
                         VerticalAlignment v_align);

  //
  // Computes the minimum size given the current theme and graphics
  gfx::Size GetPreferredSize();

  // Returns the MSAA default action of the current view. The string returned
  // describes the default action that will occur when executing
  // IAccessible::DoDefaultAction.
  bool GetAccessibleDefaultAction(std::wstring* action);

  // Returns the MSAA role of the current view. The role is what assistive
  // technologies (ATs) use to determine what behavior to expect from a given
  // control.
  bool GetAccessibleRole(VARIANT* role);

  // Set the tooltip text for this button.
  void SetTooltipText(const std::wstring& text);

  // Return the tooltip text currently used by this button.
  std::wstring GetTooltipText() const;

  // Overridden from View.
  virtual bool GetTooltipText(int x, int y, std::wstring* tooltip);
 protected:

  // Overridden to render the button.
  virtual void Paint(ChromeCanvas* canvas);

  // Returns the image to paint. This is invoked from paint and returns a value
  // from images.
  virtual SkBitmap GetImageToPaint();

  // Images.
  SkBitmap images_[kButtonStateCount];

  // Alignment State.
  HorizontalAlignment h_alignment_;
  VerticalAlignment v_alignment_;

  // The tooltip text or empty string for none.
  std::wstring tooltip_text_;

  DISALLOW_COPY_AND_ASSIGN(Button);
};

////////////////////////////////////////////////////////////////////////////////
//
// ToggleButton
//
// A togglable button.  It swaps out its graphics when toggled.
//
////////////////////////////////////////////////////////////////////////////////
class ToggleButton : public Button {
 public:
  ToggleButton();
  virtual ~ToggleButton();

  // Overridden from Button.
  virtual void SetImage(ButtonState aState, SkBitmap* anImage);

  // Like Button::SetImage(), but to set the graphics used for the
  // "has been toggled" state.  Must be called for each button state
  // before the button is toggled.
  void SetToggledImage(ButtonState state, SkBitmap* image);

  virtual bool GetTooltipText(int x, int y, std::wstring* tooltip);

  // Set the tooltip text displayed when the button is toggled.
  void SetToggledTooltipText(const std::wstring& tooltip);

  // Change the toggled state.
  void SetToggled(bool toggled);

 private:
  // The parent class's images_ member is used for the current images,
  // and this array is used to hold the alternative images.
  // We swap between the two when toggling.
  SkBitmap alternate_images_[kButtonStateCount];

  bool toggled_;

  // The parent class's tooltip_text_ is displayed when not toggled, and
  // this one is shown when toggled.
  std::wstring toggled_tooltip_text_;

  DISALLOW_EVIL_CONSTRUCTORS(ToggleButton);
};

}  // namespace views

#endif  // CHROME_VIEWS_BUTTON_H_
