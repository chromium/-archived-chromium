// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/views/border.h"

#include "base/logging.h"
#include "chrome/common/gfx/chrome_canvas.h"

namespace views {

namespace {

// A simple border with a fixed thickness and single color.
class SolidBorder : public Border {
 public:
  SolidBorder(int thickness, SkColor color);

  virtual void Paint(const View& view, ChromeCanvas* canvas) const;
  virtual void GetInsets(gfx::Insets* insets) const;

 private:
  int thickness_;
  SkColor color_;
  gfx::Insets insets_;

  DISALLOW_EVIL_CONSTRUCTORS(SolidBorder);
};

SolidBorder::SolidBorder(int thickness, SkColor color)
    : thickness_(thickness),
      color_(color),
      insets_(thickness, thickness, thickness, thickness) {
}

void SolidBorder::Paint(const View& view, ChromeCanvas* canvas) const {
  gfx::Rect clip_rect;
  if (!canvas->GetClipRect(&clip_rect))
    return;  // Empty clip rectangle, nothing to paint.

  // Top border.
  gfx::Rect border_bounds(0, 0, view.width(), insets_.top());
  if (clip_rect.Intersects(border_bounds))
    canvas->FillRectInt(color_, 0, 0, view.width(), insets_.top());
  // Left border.
  border_bounds.SetRect(0, 0, insets_.left(), view.height());
  if (clip_rect.Intersects(border_bounds))
    canvas->FillRectInt(color_, 0, 0, insets_.left(), view.height());
  // Bottom border.
  border_bounds.SetRect(0, view.height() - insets_.bottom(),
                        view.width(), insets_.bottom());
  if (clip_rect.Intersects(border_bounds))
    canvas->FillRectInt(color_, 0, view.height() - insets_.bottom(),
                        view.width(), insets_.bottom());
  // Right border.
  border_bounds.SetRect(view.width() - insets_.right(), 0,
                       insets_.right(), view.height());
  if (clip_rect.Intersects(border_bounds))
    canvas->FillRectInt(color_, view.width() - insets_.right(), 0,
                        insets_.right(), view.height());
}

void SolidBorder::GetInsets(gfx::Insets* insets) const {
  DCHECK(insets);
  insets->Set(insets_.top(), insets_.left(), insets_.bottom(), insets_.right());
}

class EmptyBorder : public Border {
 public:
  EmptyBorder(int top, int left, int bottom, int right)
      : top_(top), left_(left), bottom_(bottom), right_(right) {}

  virtual void Paint(const View& view, ChromeCanvas* canvas) const {}

  virtual void GetInsets(gfx::Insets* insets) const {
    DCHECK(insets);
    insets->Set(top_, left_, bottom_, right_);
  }

 private:
  int top_;
  int left_;
  int bottom_;
  int right_;

  DISALLOW_EVIL_CONSTRUCTORS(EmptyBorder);
};
}

Border::Border() {
}

Border::~Border() {
}

// static
Border* Border::CreateSolidBorder(int thickness, SkColor color) {
  return new SolidBorder(thickness, color);
}

// static
Border* Border::CreateEmptyBorder(int top, int left, int bottom, int right) {
  return new EmptyBorder(top, left, bottom, right);
}

}  // namespace views
