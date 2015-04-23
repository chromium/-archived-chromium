// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/controls/button/text_button.h"

#include "app/gfx/canvas.h"
#include "app/l10n_util.h"
#include "app/throb_animation.h"
#include "app/resource_bundle.h"
#include "views/controls/button/button.h"
#include "views/event.h"
#include "grit/app_resources.h"

namespace views {

// Padding between the icon and text.
static const int kIconTextPadding = 5;

// Preferred padding between text and edge
static const int kPreferredPaddingHorizontal = 6;
static const int kPreferredPaddingVertical = 5;

static const SkColor kEnabledColor = SkColorSetRGB(6, 45, 117);
static const SkColor kHighlightColor = SkColorSetARGB(200, 255, 255, 255);
static const SkColor kDisabledColor = SkColorSetRGB(161, 161, 146);

// How long the hover fade animation should last.
static const int kHoverAnimationDurationMs = 170;

////////////////////////////////////////////////////////////////////////////////
//
// TextButtonBorder - constructors, destructors, initialization
//
////////////////////////////////////////////////////////////////////////////////

TextButtonBorder::TextButtonBorder() {
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();

  hot_set_.top_left = rb.GetBitmapNamed(IDR_TEXTBUTTON_TOP_LEFT_H);
  hot_set_.top = rb.GetBitmapNamed(IDR_TEXTBUTTON_TOP_H);
  hot_set_.top_right = rb.GetBitmapNamed(IDR_TEXTBUTTON_TOP_RIGHT_H);
  hot_set_.left = rb.GetBitmapNamed(IDR_TEXTBUTTON_LEFT_H);
  hot_set_.center = rb.GetBitmapNamed(IDR_TEXTBUTTON_CENTER_H);
  hot_set_.right = rb.GetBitmapNamed(IDR_TEXTBUTTON_RIGHT_H);
  hot_set_.bottom_left = rb.GetBitmapNamed(IDR_TEXTBUTTON_BOTTOM_LEFT_H);
  hot_set_.bottom = rb.GetBitmapNamed(IDR_TEXTBUTTON_BOTTOM_H);
  hot_set_.bottom_right = rb.GetBitmapNamed(IDR_TEXTBUTTON_BOTTOM_RIGHT_H);

  pushed_set_.top_left = rb.GetBitmapNamed(IDR_TEXTBUTTON_TOP_LEFT_P);
  pushed_set_.top = rb.GetBitmapNamed(IDR_TEXTBUTTON_TOP_P);
  pushed_set_.top_right = rb.GetBitmapNamed(IDR_TEXTBUTTON_TOP_RIGHT_P);
  pushed_set_.left = rb.GetBitmapNamed(IDR_TEXTBUTTON_LEFT_P);
  pushed_set_.center = rb.GetBitmapNamed(IDR_TEXTBUTTON_CENTER_P);
  pushed_set_.right = rb.GetBitmapNamed(IDR_TEXTBUTTON_RIGHT_P);
  pushed_set_.bottom_left = rb.GetBitmapNamed(IDR_TEXTBUTTON_BOTTOM_LEFT_P);
  pushed_set_.bottom = rb.GetBitmapNamed(IDR_TEXTBUTTON_BOTTOM_P);
  pushed_set_.bottom_right = rb.GetBitmapNamed(IDR_TEXTBUTTON_BOTTOM_RIGHT_P);
}

TextButtonBorder::~TextButtonBorder() {
}

////////////////////////////////////////////////////////////////////////////////
//
// TextButtonBackground - painting
//
////////////////////////////////////////////////////////////////////////////////

void TextButtonBorder::Paint(const View& view, gfx::Canvas* canvas) const {
  const TextButton* mb = static_cast<const TextButton*>(&view);
  int state = mb->state();

  // TextButton takes care of deciding when to call Paint.
  const MBBImageSet* set = &hot_set_;
  if (state == TextButton::BS_PUSHED)
    set = &pushed_set_;

  if (set) {
    gfx::Rect bounds = view.bounds();

    // Draw the top left image
    canvas->DrawBitmapInt(*set->top_left, 0, 0);

    // Tile the top image
    canvas->TileImageInt(
        *set->top,
        set->top_left->width(),
        0,
        bounds.width() - set->top_right->width() - set->top_left->width(),
        set->top->height());

    // Draw the top right image
    canvas->DrawBitmapInt(*set->top_right,
                          bounds.width() - set->top_right->width(), 0);

    // Tile the left image
    canvas->TileImageInt(
        *set->left,
        0,
        set->top_left->height(),
        set->top_left->width(),
        bounds.height() - set->top->height() - set->bottom_left->height());

    // Tile the center image
    canvas->TileImageInt(
        *set->center,
        set->left->width(),
        set->top->height(),
        bounds.width() - set->right->width() - set->left->width(),
        bounds.height() - set->bottom->height() - set->top->height());

    // Tile the right image
    canvas->TileImageInt(
        *set->right,
        bounds.width() - set->right->width(),
        set->top_right->height(),
        bounds.width(),
        bounds.height() - set->bottom_right->height() -
            set->top_right->height());

    // Draw the bottom left image
    canvas->DrawBitmapInt(*set->bottom_left,
                          0,
                          bounds.height() - set->bottom_left->height());

    // Tile the bottom image
    canvas->TileImageInt(
        *set->bottom,
        set->bottom_left->width(),
        bounds.height() - set->bottom->height(),
        bounds.width() - set->bottom_right->width() -
            set->bottom_left->width(),
        set->bottom->height());

    // Draw the bottom right image
    canvas->DrawBitmapInt(*set->bottom_right,
                          bounds.width() - set->bottom_right->width(),
                          bounds.height() -  set->bottom_right->height());
  } else {
    // Do nothing
  }
}

void TextButtonBorder::GetInsets(gfx::Insets* insets) const {
  insets->Set(kPreferredPaddingVertical, kPreferredPaddingHorizontal,
              kPreferredPaddingVertical, kPreferredPaddingHorizontal);
}

////////////////////////////////////////////////////////////////////////////////
// TextButton, public:

TextButton::TextButton(ButtonListener* listener, const std::wstring& text)
    : CustomButton(listener),
      alignment_(ALIGN_LEFT),
      font_(ResourceBundle::GetSharedInstance().GetFont(
          ResourceBundle::BaseFont)),
      color_(kEnabledColor),
      color_enabled_(kEnabledColor),
      color_disabled_(kDisabledColor),
      color_highlight_(kHighlightColor),
      max_width_(0) {
  SetText(text);
  set_border(new TextButtonBorder);
  SetAnimationDuration(kHoverAnimationDurationMs);
}

TextButton::~TextButton() {
}

void TextButton::SetText(const std::wstring& text) {
  text_ = text;
  // Update our new current and max text size
  text_size_.SetSize(font_.GetStringWidth(text_), font_.height());
  max_text_size_.SetSize(std::max(max_text_size_.width(), text_size_.width()),
                         std::max(max_text_size_.height(),
                                  text_size_.height()));
}

void TextButton::SetIcon(const SkBitmap& icon) {
  icon_ = icon;
}

void TextButton::SetEnabledColor(SkColor color) {
  color_enabled_ = color;
  UpdateColor();
}

void TextButton::SetDisabledColor(SkColor color) {
  color_disabled_ = color;
  UpdateColor();
}

void TextButton::SetHighlightColor(SkColor color) {
  color_highlight_ = color;
}

void TextButton::ClearMaxTextSize() {
  max_text_size_ = text_size_;
}

void TextButton::Paint(gfx::Canvas* canvas, bool for_drag) {
  if (!for_drag) {
    PaintBackground(canvas);

    if (hover_animation_->IsAnimating()) {
      // Draw the hover bitmap into an offscreen buffer, then blend it
      // back into the current canvas.
      canvas->saveLayerAlpha(NULL,
          static_cast<int>(hover_animation_->GetCurrentValue() * 255),
          SkCanvas::kARGB_NoClipLayer_SaveFlag);
      canvas->drawARGB(0, 255, 255, 255, SkXfermode::kClear_Mode);
      PaintBorder(canvas);
      canvas->restore();
    } else if (state_ == BS_HOT || state_ == BS_PUSHED) {
      PaintBorder(canvas);
    }

    PaintFocusBorder(canvas);
  }

  gfx::Insets insets = GetInsets();
  int available_width = width() - insets.width();
  int available_height = height() - insets.height();
  // Use the actual text (not max) size to properly center the text.
  int content_width = text_size_.width();
  if (icon_.width() > 0) {
    content_width += icon_.width();
    if (!text_.empty())
      content_width += kIconTextPadding;
  }
  // Place the icon along the left edge.
  int icon_x;
  if (alignment_ == ALIGN_LEFT) {
    icon_x = insets.left();
  } else if (alignment_ == ALIGN_RIGHT) {
    icon_x = available_width - content_width;
  } else {
    icon_x =
        std::max(0, (available_width - content_width) / 2) + insets.left();
  }
  int text_x = icon_x;
  if (icon_.width() > 0)
    text_x += icon_.width() + kIconTextPadding;
  const int text_width = std::min(text_size_.width(),
                                  width() - insets.right() - text_x);
  int text_y = (available_height - text_size_.height()) / 2 + insets.top();

  if (text_width > 0) {
    // Because the text button can (at times) draw multiple elements on the
    // canvas, we can not mirror the button by simply flipping the canvas as
    // doing this will mirror the text itself. Flipping the canvas will also
    // make the icons look wrong because icons are almost always represented as
    // direction insentisive bitmaps and such bitmaps should never be flipped
    // horizontally.
    //
    // Due to the above, we must perform the flipping manually for RTL UIs.
    gfx::Rect text_bounds(text_x, text_y, text_width, text_size_.height());
    text_bounds.set_x(MirroredLeftPointForRect(text_bounds));

    if (for_drag) {
#if defined(OS_WIN)
      // TODO(erg): Either port DrawStringWithHalo to linux or find an
      // alternative here.
      canvas->DrawStringWithHalo(text_, font_, color_, color_highlight_,
                                 text_bounds.x(),
                                 text_bounds.y(),
                                 text_bounds.width(),
                                 text_bounds.height(),
                                 l10n_util::DefaultCanvasTextAlignment());
#endif
    } else {
      // Draw bevel highlight
      canvas->DrawStringInt(text_,
                            font_,
                            color_highlight_,
                            text_bounds.x() + 1,
                            text_bounds.y() + 1,
                            text_bounds.width(),
                            text_bounds.height());

      canvas->DrawStringInt(text_,
                            font_,
                            color_,
                            text_bounds.x(),
                            text_bounds.y(),
                            text_bounds.width(),
                            text_bounds.height());
    }
  }

  if (icon_.width() > 0) {
    int icon_y = (available_height - icon_.height()) / 2 + insets.top();

    // Mirroring the icon position if necessary.
    gfx::Rect icon_bounds(icon_x, icon_y, icon_.width(), icon_.height());
    icon_bounds.set_x(MirroredLeftPointForRect(icon_bounds));
    canvas->DrawBitmapInt(icon_, icon_bounds.x(), icon_bounds.y());
  }
}

void TextButton::UpdateColor() {
  color_ = IsEnabled() ? color_enabled_ : color_disabled_;
}

////////////////////////////////////////////////////////////////////////////////
// TextButton, View overrides:

gfx::Size TextButton::GetPreferredSize() {
  gfx::Insets insets = GetInsets();

  // Use the max size to set the button boundaries.
  gfx::Size prefsize(max_text_size_.width() + icon_.width() + insets.width(),
                     std::max(max_text_size_.height(), icon_.height()) +
                         insets.height());

  if (icon_.width() > 0 && !text_.empty())
    prefsize.Enlarge(kIconTextPadding, 0);

  if (max_width_ > 0)
    prefsize.set_width(std::min(max_width_, prefsize.width()));

  return prefsize;
}

gfx::Size TextButton::GetMinimumSize() {
  return max_text_size_;
}

void TextButton::SetEnabled(bool enabled) {
  if (enabled == IsEnabled())
    return;
  CustomButton::SetEnabled(enabled);
  UpdateColor();
  SchedulePaint();
}

bool TextButton::OnMousePressed(const MouseEvent& e) {
  return true;
}

void TextButton::Paint(gfx::Canvas* canvas) {
  Paint(canvas, false);
}

}  // namespace views
