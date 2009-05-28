// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "app/gfx/font.h"

#include "app/gfx/canvas.h"

#include "base/logging.h"
#include "base/sys_string_conversions.h"

#include "third_party/skia/include/core/SkTypeface.h"
#include "third_party/skia/include/core/SkPaint.h"

namespace gfx {

Font::Font(const Font& other) {
  CopyFont(other);
}

Font& Font::operator=(const Font& other) {
  CopyFont(other);
  return *this;
}

Font::Font(SkTypeface* tf, const std::wstring& font_family, int font_size,
           int style)
    : typeface_helper_(new SkAutoUnref(tf)),
      typeface_(tf),
      font_family_(font_family),
      font_size_(font_size),
      style_(style) {
  tf->ref();
  calculateMetrics();
}

void Font::calculateMetrics() {
  SkPaint paint;
  SkPaint::FontMetrics metrics;

  PaintSetup(&paint);
  paint.getFontMetrics(&metrics);

  ascent_ = SkScalarRound(-metrics.fAscent);
  height_ = SkScalarRound(-metrics.fAscent + metrics.fDescent +
                          metrics.fLeading);

  if (metrics.fAvgCharWidth) {
    avg_width_ = SkScalarRound(metrics.fAvgCharWidth);
  } else {
    static const char x_char = 'x';
    paint.setTextEncoding(SkPaint::kUTF8_TextEncoding);
    SkScalar width = paint.measureText(&x_char, 1);

    avg_width_ = static_cast<int>(ceilf(SkScalarToFloat(width)));
  }
}

void Font::CopyFont(const Font& other) {
  typeface_helper_.reset(new SkAutoUnref(other.typeface_));
  typeface_ = other.typeface_;
  typeface_->ref();
  font_family_ = other.font_family_;
  font_size_ = other.font_size_;
  style_ = other.style_;
  height_ = other.height_;
  ascent_ = other.ascent_;
  avg_width_ = other.avg_width_;
}

int Font::height() const {
  return height_;
}

int Font::baseline() const {
  return ascent_;
}

int Font::ave_char_width() const {
  return avg_width_;
}

Font Font::CreateFont(const std::wstring& font_family, int font_size) {
  DCHECK_GT(font_size, 0);

  SkTypeface* tf = SkTypeface::CreateFromName(
      base::SysWideToUTF8(font_family).c_str(), SkTypeface::kNormal);
  // Temporary CHECK for tracking down
  // http://code.google.com/p/chromium/issues/detail?id=12530
  CHECK(tf) << "Could not find font: " << base::SysWideToUTF8(font_family);
  SkAutoUnref tf_helper(tf);

  return Font(tf, font_family, font_size, NORMAL);
}

Font Font::DeriveFont(int size_delta, int style) const {
  // If the delta is negative, if must not push the size below 1
  if (size_delta < 0) {
    DCHECK_LT(-size_delta, font_size_);
  }

  if (style == style_) {
    // Fast path, we just use the same typeface at a different size
    return Font(typeface_, font_family_, font_size_ + size_delta, style_);
  }

  // If the style has changed we may need to load a new face
  int skstyle = SkTypeface::kNormal;
  if (BOLD & style)
    skstyle |= SkTypeface::kBold;
  if (ITALIC & style)
    skstyle |= SkTypeface::kItalic;

  SkTypeface* tf = SkTypeface::CreateFromName(
      base::SysWideToUTF8(font_family_).c_str(),
      static_cast<SkTypeface::Style>(skstyle));
  SkAutoUnref tf_helper(tf);

  return Font(tf, font_family_, font_size_ + size_delta, skstyle);
}

void Font::PaintSetup(SkPaint* paint) const {
  paint->setAntiAlias(false);
  paint->setSubpixelText(false);
  paint->setTextSize(SkFloatToScalar(font_size_));
  paint->setTypeface(typeface_);
  paint->setFakeBoldText((BOLD & style_) && !typeface_->isBold());
  paint->setTextSkewX((ITALIC & style_) && !typeface_->isItalic() ?
                      -SK_Scalar1/4 : 0);
}

int Font::GetStringWidth(const std::wstring& text) const {
  int width = 0, height = 0;

  Canvas::SizeStringInt(text, *this, &width, &height, 0);
  return width;
}

int Font::GetExpectedTextWidth(int length) const {
  return length * avg_width_;
}


int Font::style() const {
  return style_;
}

std::wstring Font::FontName() {
  return font_family_;
}

int Font::FontSize() {
  return font_size_;
}

NativeFont Font::nativeFont() const {
  return typeface_;
}

}  // namespace gfx
