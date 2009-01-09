// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/gfx/chrome_font.h"

#include "base/logging.h"
#include "base/sys_string_conversions.h"

#include "skia/include/SkTypeface.h"
#include "skia/include/SkPaint.h"

ChromeFont::ChromeFont(const ChromeFont& other) {
  CopyChromeFont(other);
}

ChromeFont::ChromeFont(SkTypeface* tf, const std::wstring& font_name,
                       int font_size, int style)
    : typeface_helper_(new SkAutoUnref(tf)),
      typeface_(tf),
      font_name_(font_name),
      font_size_(font_size),
      style_(style) {
  calculateMetrics();
}

void ChromeFont::calculateMetrics() {
  SkPaint paint;
  SkPaint::FontMetrics metrics;

  PaintSetup(&paint);
  paint.getFontMetrics(&metrics);

  if (metrics.fVDMXMetricsValid) {
    ascent_ = metrics.fVDMXAscent;
    height_ = ascent_ + metrics.fVDMXDescent;
  } else {
    ascent_ = SkScalarRound(-metrics.fAscent);
    height_ = SkScalarRound(metrics.fHeight);
  }

  if (metrics.fAvgCharWidth) {
    avg_width_ = SkScalarRound(metrics.fAvgCharWidth);
  } else {
    static const char x_char = 'x';
    paint.setTextEncoding(SkPaint::kUTF8_TextEncoding);
    SkScalar width = paint.measureText(&x_char, 1);

    avg_width_ = static_cast<int>(ceilf(SkScalarToFloat(width)));
  }
}

void ChromeFont::CopyChromeFont(const ChromeFont& other) {
  typeface_helper_.reset(new SkAutoUnref(other.typeface_));
  typeface_ = other.typeface_;
  font_name_ = other.font_name_;
  font_size_ = other.font_size_;
  style_ = other.style_;
  height_ = other.height_;
  ascent_ = other.ascent_;
  avg_width_ = other.avg_width_;
}

int ChromeFont::height() const {
  return height_;
}

int ChromeFont::baseline() const {
  return ascent_;
}

int ChromeFont::ave_char_width() const {
  return avg_width_;
}

ChromeFont ChromeFont::CreateFont(const std::wstring& font_name, int font_size) {
  DCHECK_GT(font_size, 0);

  SkTypeface* tf = SkTypeface::Create(base::SysWideToUTF8(font_name).c_str(),
                                      SkTypeface::kNormal);
  SkAutoUnref tf_helper(tf);

  return ChromeFont(tf, font_name, font_size, NORMAL);
}

ChromeFont ChromeFont::DeriveFont(int size_delta, int style) const {
  // If the delta is negative, if must not push the size below 1
  if (size_delta < 0) {
    DCHECK_LT(-size_delta, font_size_);
  }

  if (style == style_) {
    // Fast path, we just use the same typeface at a different size
    return ChromeFont(typeface_, font_name_, font_size_ + size_delta, style_);
  }

  // If the style has changed we may need to load a new face
  int skstyle = SkTypeface::kNormal;
  if (BOLD & style)
    skstyle |= SkTypeface::kBold;
  if (ITALIC & style)
    skstyle |= SkTypeface::kItalic;

  SkTypeface* tf = SkTypeface::Create(base::SysWideToUTF8(font_name_).c_str(),
                                      static_cast<SkTypeface::Style>(skstyle));
  SkAutoUnref tf_helper(tf);

  return ChromeFont(tf, font_name_, font_size_ + size_delta, skstyle);
}

void ChromeFont::PaintSetup(SkPaint* paint) const {
  paint->setAntiAlias(false);
  paint->setSubpixelText(false);
  paint->setTextSize(SkFloatToScalar(font_size_));
  paint->setTypeface(typeface_);
  paint->setFakeBoldText((BOLD & style_) && !typeface_->isBold());
  paint->setTextSkewX((ITALIC & style_) && !typeface_->isItalic() ?
                      -SK_Scalar1/4 : 0);
}

int ChromeFont::GetStringWidth(const std::wstring& text) const {
  const std::string utf8(base::SysWideToUTF8(text));

  SkPaint paint;
  PaintSetup(&paint);
  paint.setTextEncoding(SkPaint::kUTF8_TextEncoding);
  SkScalar width = paint.measureText(utf8.data(), utf8.size());

  return static_cast<int>(ceilf(SkScalarToFloat(width)));
}

int ChromeFont::GetExpectedTextWidth(int length) const {
  return length * avg_width_;
}


int ChromeFont::style() const {
  return style_;
}

std::wstring ChromeFont::FontName() {
  return font_name_;
}

int ChromeFont::FontSize() {
  return font_size_;
}

NativeFont ChromeFont::nativeFont() const {
  return typeface_;
}
