// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "chrome/common/gfx/chrome_canvas.h"
#include "chrome/views/image_view.h"

namespace ChromeViews {

ImageView::ImageView()
    : image_size_set_(false),
      horiz_alignment_(CENTER),
      vert_alignment_(CENTER) {
}

ImageView::~ImageView() {
}

void ImageView::SetImage(const SkBitmap& bm) {
  image_ = bm;
  SchedulePaint();
}

void ImageView::SetImage(SkBitmap* bm) {
  if (bm) {
    SetImage(*bm);
  } else {
    SkBitmap t;
    SetImage(t);
  }
}

const SkBitmap& ImageView::GetImage() {
  return image_;
}

void ImageView::SetImageSize(const CSize& image_size) {
  image_size_set_ = true;
  image_size_ = image_size;
}

bool ImageView::GetImageSize(CSize* image_size) {
  DCHECK(image_size);
  if (image_size_set_)
    *image_size = image_size_;
  return image_size_set_;
}

void ImageView::ResetImageSize() {
  image_size_set_ = false;
}

void ImageView::GetPreferredSize(CSize* out) {
  DCHECK(out);
  if (image_size_set_) {
    GetImageSize(out);
  } else {
    out->cx = image_.width();
    out->cy = image_.height();
  }
}

void ImageView::ComputeImageOrigin(int image_width, int image_height,
                                   int *x, int *y) {
  // In order to properly handle alignment of images in RTL locales, we need
  // to flip the meaning of trailing and leading. For example, if the
  // horizontal alignment is set to trailing, then we'll use left alignment for
  // the image instead of right alignment if the UI layout is RTL.
  Alignment actual_horiz_alignment = horiz_alignment_;
  if (UILayoutIsRightToLeft()) {
    if (horiz_alignment_ == TRAILING)
      actual_horiz_alignment = LEADING;
    if (horiz_alignment_ == LEADING)
      actual_horiz_alignment = TRAILING;
  }

  switch(actual_horiz_alignment) {
    case LEADING:
      *x = 0;
      break;
    case TRAILING:
      *x = GetWidth() - image_width;
      break;
    case CENTER:
      *x = (GetWidth() - image_width) / 2;
      break;
    default:
      NOTREACHED();
  }

  switch (vert_alignment_) {
    case LEADING:
      *y = 0;
      break;
    case TRAILING:
      *y = GetHeight() - image_height;
      break;
    case CENTER:
      *y = (GetHeight() - image_height) / 2;
      break;
    default:
      NOTREACHED();
  }
}

void ImageView::Paint(ChromeCanvas* canvas) {
  View::Paint(canvas);
  int image_width = image_.width();
  int image_height = image_.height();

  if (image_width == 0 || image_height == 0)
    return;

  int x, y;
  if (image_size_set_ &&
      (image_size_.cx != image_width || image_size_.cy != image_height)) {
    // Resize case
    image_.buildMipMap(false);
    ComputeImageOrigin(image_size_.cx, image_size_.cy, &x, &y);
    canvas->DrawBitmapInt(image_, 0, 0, image_width, image_height,
                          x, y, image_size_.cx, image_size_.cy,
                          true);
  } else {
    ComputeImageOrigin(image_width, image_height, &x, &y);
    canvas->DrawBitmapInt(image_, x, y);
  }
}

void ImageView::SetHorizontalAlignment(Alignment ha) {
  if (ha != horiz_alignment_) {
    horiz_alignment_ = ha;
    SchedulePaint();
  }
}

ImageView::Alignment ImageView::GetHorizontalAlignment() {
  return horiz_alignment_;
}

void ImageView::SetVerticalAlignment(Alignment va) {
  if (va != vert_alignment_) {
    vert_alignment_ = va;
    SchedulePaint();
  }
}

ImageView::Alignment ImageView::GetVerticalAlignment() {
  return vert_alignment_;
}

void ImageView::SetTooltipText(const std::wstring& tooltip) {
  tooltip_text_ = tooltip;
}

std::wstring ImageView::GetTooltipText() {
  return tooltip_text_;
}

bool ImageView::GetTooltipText(int x, int y, std::wstring* tooltip) {
  if (tooltip_text_.empty()) {
    return false;
  } else {
    * tooltip =  GetTooltipText();
    return true;
  }
}

}

