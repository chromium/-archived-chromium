// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_VIEWS_IMAGE_BUTTON_H_
#define CHROME_VIEWS_IMAGE_BUTTON_H_

#include "chrome/views/custom_button.h"
#include "skia/include/SkBitmap.h"

namespace views {

// An image button.
class ImageButton : public CustomButton {
 public:
  explicit ImageButton(ButtonListener* listener);
  virtual ~ImageButton();

  // Set the image the button should use for the provided state.
  virtual void SetImage(ButtonState aState, SkBitmap* anImage);

  enum HorizontalAlignment { ALIGN_LEFT = 0,
                             ALIGN_CENTER,
                             ALIGN_RIGHT, };

  enum VerticalAlignment { ALIGN_TOP = 0,
                           ALIGN_MIDDLE,
                           ALIGN_BOTTOM };

  // Sets how the image is laid out within the button's bounds.
  void SetImageAlignment(HorizontalAlignment h_align,
                         VerticalAlignment v_align);

  // Overridden from View:
  virtual gfx::Size GetPreferredSize();
  virtual void Paint(ChromeCanvas* canvas);

 protected:
  // Returns the image to paint. This is invoked from paint and returns a value
  // from images.
  virtual SkBitmap GetImageToPaint();

  // The images used to render the different states of this button.
  SkBitmap images_[BS_COUNT];

 private:
  // Image alignment.
  HorizontalAlignment h_alignment_;
  VerticalAlignment v_alignment_;

  DISALLOW_COPY_AND_ASSIGN(ImageButton);
};

////////////////////////////////////////////////////////////////////////////////
//
// ToggleImageButton
//
// A toggle-able ImageButton.  It swaps out its graphics when toggled.
//
////////////////////////////////////////////////////////////////////////////////
class ToggleImageButton : public ImageButton {
 public:
  ToggleImageButton(ButtonListener* listener);
  virtual ~ToggleImageButton();

  // Change the toggled state.
  void SetToggled(bool toggled);

  // Like Button::SetImage(), but to set the graphics used for the
  // "has been toggled" state.  Must be called for each button state
  // before the button is toggled.
  void SetToggledImage(ButtonState state, SkBitmap* image);

  // Set the tooltip text displayed when the button is toggled.
  void SetToggledTooltipText(const std::wstring& tooltip);

  // Overridden from ImageButton:
  virtual void SetImage(ButtonState aState, SkBitmap* anImage);

  // Overridden from View:
  virtual bool GetTooltipText(int x, int y, std::wstring* tooltip);

 private:
  // The parent class's images_ member is used for the current images,
  // and this array is used to hold the alternative images.
  // We swap between the two when toggling.
  SkBitmap alternate_images_[BS_COUNT];

  // True if the button is currently toggled.
  bool toggled_;

  // The parent class's tooltip_text_ is displayed when not toggled, and
  // this one is shown when toggled.
  std::wstring toggled_tooltip_text_;

  DISALLOW_EVIL_CONSTRUCTORS(ToggleImageButton);
};

}  // namespace views

#endif  // CHROME_VIEWS_IMAGE_BUTTON_H_
