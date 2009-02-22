// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/views/button.h"

#include <atlbase.h>
#include <atlapp.h>

#include "chrome/app/chrome_dll_resource.h"
#include "chrome/common/gfx/chrome_canvas.h"
#include "chrome/common/l10n_util.h"
#include "chrome/common/throb_animation.h"
#include "chrome/views/event.h"
#include "grit/generated_resources.h"
#include "skia/ext/image_operations.h"

namespace views {

static const int kDefaultWidth = 16;   // Default button width if no theme.
static const int kDefaultHeight = 14;  // Default button height if no theme.

////////////////////////////////////////////////////////////////////////////////
//
// Button - constructors, destructors, initialization
//
////////////////////////////////////////////////////////////////////////////////

Button::Button() : BaseButton(),
                   h_alignment_(ALIGN_LEFT),
                   v_alignment_(ALIGN_TOP) {
  // By default, we request that the ChromeCanvas passed to our View::Paint()
  // implementation is flipped horizontally so that the button's bitmaps are
  // mirrored when the UI directionality is right-to-left.
  EnableCanvasFlippingForRTLUI(true);
}

Button::~Button() {
}

////////////////////////////////////////////////////////////////////////////////
//
// Button - properties
//
////////////////////////////////////////////////////////////////////////////////

void Button::SetImage(ButtonState aState, SkBitmap* anImage) {
  images_[aState] = anImage ? *anImage : SkBitmap();
}

void Button::SetImageAlignment(HorizontalAlignment h_align,
                               VerticalAlignment v_align) {
  h_alignment_ = h_align;
  v_alignment_ = v_align;
  SchedulePaint();
}

gfx::Size Button::GetPreferredSize() {
  if (!images_[BS_NORMAL].isNull())
    return gfx::Size(images_[BS_NORMAL].width(), images_[BS_NORMAL].height());
  return gfx::Size(kDefaultWidth, kDefaultHeight);
}

// Set the tooltip text for this button.
void Button::SetTooltipText(const std::wstring& text) {
  tooltip_text_ = text;
  TooltipTextChanged();
}

// Return the tooltip text currently used by this button.
std::wstring Button::GetTooltipText() const {
  return tooltip_text_;
}

////////////////////////////////////////////////////////////////////////////////
//
// Button - painting
//
////////////////////////////////////////////////////////////////////////////////

void Button::Paint(ChromeCanvas* canvas) {
  View::Paint(canvas);
  SkBitmap img = GetImageToPaint();

  if (!img.isNull()) {
    int x = 0, y = 0;

    if (h_alignment_ == ALIGN_CENTER)
      x = (width() - img.width()) / 2;
    else if (h_alignment_ == ALIGN_RIGHT)
      x = width() - img.width();

    if (v_alignment_ == ALIGN_MIDDLE)
      y = (height() - img.height()) / 2;
    else if (v_alignment_ == ALIGN_BOTTOM)
      y = height() - img.height();

    canvas->DrawBitmapInt(img, x, y);
  }
  PaintFocusBorder(canvas);
}

SkBitmap Button::GetImageToPaint() {
  SkBitmap img;

  if (!images_[BS_HOT].isNull() && hover_animation_->IsAnimating()) {
    img = skia::ImageOperations::CreateBlendedBitmap(images_[BS_NORMAL],
              images_[BS_HOT], hover_animation_->GetCurrentValue());
  } else {
    img = images_[GetState()];
  }

  return !img.isNull() ? img : images_[BS_NORMAL];
}

////////////////////////////////////////////////////////////////////////////////
//
// Button - accessibility
//
////////////////////////////////////////////////////////////////////////////////

bool Button::GetAccessibleDefaultAction(std::wstring* action) {
  DCHECK(action);

  action->assign(l10n_util::GetString(IDS_ACCACTION_PRESS));
  return true;
}

bool Button::GetAccessibleRole(VARIANT* role) {
  DCHECK(role);

  role->vt = VT_I4;
  role->lVal = ROLE_SYSTEM_PUSHBUTTON;
  return true;
}

bool Button::GetTooltipText(int x, int y, std::wstring* tooltip) {
  if (tooltip_text_.empty())
    return false;

  *tooltip = tooltip_text_;
  return true;
}

////////////////////////////////////////////////////////////////////////////////
//
// ToggleButton
//
////////////////////////////////////////////////////////////////////////////////
ToggleButton::ToggleButton() : Button(), toggled_(false) {
}

ToggleButton::~ToggleButton() {
}

void ToggleButton::SetImage(ButtonState state, SkBitmap* image) {
  if (toggled_) {
    alternate_images_[state] = image ? *image : SkBitmap();
  } else {
    images_[state] = image ? *image : SkBitmap();
    if (state_ == state)
      SchedulePaint();
  }
}

void ToggleButton::SetToggledImage(ButtonState state, SkBitmap* image) {
  if (toggled_) {
    images_[state] = image ? *image : SkBitmap();
    if (state_ == state)
      SchedulePaint();
  } else {
    alternate_images_[state] = image ? *image : SkBitmap();
  }
}

bool ToggleButton::GetTooltipText(int x, int y, std::wstring* tooltip) {
  if (!toggled_ || toggled_tooltip_text_.empty())
    return Button::GetTooltipText(x, y, tooltip);

  *tooltip = toggled_tooltip_text_;
  return true;
}

void ToggleButton::SetToggled(bool toggled) {
  if (toggled == toggled_)
    return;

  for (int i = 0; i < kButtonStateCount; ++i) {
    SkBitmap temp = images_[i];
    images_[i] = alternate_images_[i];
    alternate_images_[i] = temp;
  }
  toggled_ = toggled;
  SchedulePaint();
}

void ToggleButton::SetToggledTooltipText(const std::wstring& tooltip) {
  toggled_tooltip_text_.assign(tooltip);
}

}  // namespace views

