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

#include "chrome/views/border.h"

#include "base/logging.h"

namespace ChromeViews {


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
  gfx::Rect border_bounds(0, 0, view.GetWidth(), insets_.top());
  if (clip_rect.Intersects(border_bounds))
    canvas->FillRectInt(color_, 0, 0, view.GetWidth(), insets_.top());
  // Left border.
  border_bounds.SetRect(0, 0, insets_.left(), view.GetHeight());
  if (clip_rect.Intersects(border_bounds))
    canvas->FillRectInt(color_, 0, 0, insets_.left(), view.GetHeight());
  // Bottom border.
  border_bounds.SetRect(0, view.GetHeight() - insets_.bottom(),
                        view.GetWidth(), insets_.bottom());
  if (clip_rect.Intersects(border_bounds))
    canvas->FillRectInt(color_, 0, view.GetHeight() - insets_.bottom(),
                        view.GetWidth(), insets_.bottom());
  // Right border.
  border_bounds.SetRect(view.GetWidth() - insets_.right(), 0,
                       insets_.right(), view.GetHeight());
  if (clip_rect.Intersects(border_bounds))
    canvas->FillRectInt(color_, view.GetWidth() - insets_.right(), 0,
                        insets_.right(), view.GetHeight());
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

} // namespace
