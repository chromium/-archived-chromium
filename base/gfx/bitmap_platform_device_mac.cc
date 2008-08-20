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

#include <time.h>

#include "SkMatrix.h"
#include "SkRegion.h"
#include "SkUtils.h"

#include "base/gfx/bitmap_platform_device_mac.h"
#include "base/gfx/skia_utils_mac.h"
#include "base/logging.h"

namespace gfx {

namespace {

// Constrains position and size to fit within available_size. If |size| is -1,
// all the |available_size| is used. Returns false if the position is out of
// |available_size|.
bool Constrain(int available_size, int* position, int *size) {
  if (*size < -2)
    return false;
  
  if (*position < 0) {
    if (*size != -1)
      *size += *position;
    *position = 0;
  }
  if (*size == 0 || *position >= available_size)
    return false;
  
  if (*size > 0) {
    int overflow = (*position + *size) - available_size;
    if (overflow > 0) {
      *size -= overflow;
    }
  } else {
    // Fill up available size.
    *size = available_size - *position;
  }
  return true;
}

} // namespace

class BitmapPlatformDeviceMac::BitmapPlatformDeviceMacData
    : public base::RefCounted<BitmapPlatformDeviceMacData> {
 public:
  explicit BitmapPlatformDeviceMacData(CGContextRef bitmap);

  // Create/destroy CoreGraphics context for our bitmap data.
  CGContextRef GetBitmapContext() {
    return bitmap_context_;
  }

  void ReleaseBitmapContext() {
    DCHECK(bitmap_context_);
    CGContextRelease(bitmap_context_);
    bitmap_context_ = NULL;
  }

  // Sets the transform and clip operations. This will not update the CGContext,
  // but will mark the config as dirty. The next call of LoadConfig will
  // pick up these changes.
  void SetTransform(const SkMatrix& t);
  void SetClipRegion(const SkRegion& region);

  // Loads the current transform (taking into account offset_*_) and clip
  // into the DC. Can be called even when |bitmap_context_| is NULL (will be
  // a NOP).
  void LoadConfig();

  // Lazily-created graphics context used to draw into the bitmap.
  CGContextRef bitmap_context_;

  // Additional offset applied to the transform. See setDeviceOffset().
  int offset_x_;
  int offset_y_;

  // True when there is a transform or clip that has not been set to the
  // CGContext.  The CGContext is retrieved for every text operation, and the
  // transform and clip do not change as much. We can save time by not loading
  // the clip and transform for every one.
  bool config_dirty_;

  // Translation assigned to the CGContext: we need to keep track of this
  // separately so it can be updated even if the CGContext isn't created yet.
  SkMatrix transform_;

  // The current clipping
  SkRegion clip_region_;

 private:
  friend class base::RefCounted<BitmapPlatformDeviceMacData>;
  ~BitmapPlatformDeviceMacData() {
    if (bitmap_context_)
      CGContextRelease(bitmap_context_);
  }

  DISALLOW_COPY_AND_ASSIGN(BitmapPlatformDeviceMacData);
};

BitmapPlatformDeviceMac::\
    BitmapPlatformDeviceMacData::BitmapPlatformDeviceMacData(
    CGContextRef bitmap)
    : bitmap_context_(bitmap),
      offset_x_(0),
      offset_y_(0),
      config_dirty_(true) {  // Want to load the config next time.
  DCHECK(bitmap_context_);
  // Initialize the clip region to the entire bitmap.  
  
  SkIRect rect;
  rect.set(0, 0,
           CGBitmapContextGetWidth(bitmap_context_),
           CGBitmapContextGetHeight(bitmap_context_));
  SkRegion region(rect);
  SetClipRegion(region);
  CGContextRetain(bitmap_context_);
}

void BitmapPlatformDeviceMac::BitmapPlatformDeviceMacData::SetTransform(
    const SkMatrix& t) {
  transform_ = t;
  config_dirty_ = true;
}

void BitmapPlatformDeviceMac::BitmapPlatformDeviceMacData::SetClipRegion(
    const SkRegion& region) {
  clip_region_ = region;
  config_dirty_ = true;
}

void BitmapPlatformDeviceMac::BitmapPlatformDeviceMacData::LoadConfig() {
  if (!config_dirty_ || !bitmap_context_)
    return;  // Nothing to do.
  config_dirty_ = false;

  // Transform.
  SkMatrix t(transform_);
  t.postTranslate(SkIntToScalar(-offset_x_), SkIntToScalar(-offset_y_));
  LoadTransformToCGContext(bitmap_context_, t);

  // TODO(brettw) we should support more than just rect clipping here.
  SkIRect rect = clip_region_.getBounds();
  rect.offset(-offset_x_, -offset_y_);

  CGContextClipToRect(bitmap_context_, SkIRectToCGRect(rect));
}


// We use this static factory function instead of the regular constructor so
// that we can create the pixel data before calling the constructor. This is
// required so that we can call the base class' constructor with the pixel
// data.
BitmapPlatformDeviceMac* BitmapPlatformDeviceMac::Create(CGContextRef context,
                                                         int width,
                                                         int height,
                                                         bool is_opaque) {
  // each pixel is 4 bytes (RGBA):
  void* data = malloc(height * width * 4);
  if (!data) return NULL;

  SkBitmap bitmap;
  bitmap.setConfig(SkBitmap::kARGB_8888_Config, width, height);
  bitmap.setPixels(data);
  bitmap.setIsOpaque(is_opaque);

  if (is_opaque) {
#ifndef NDEBUG
    // To aid in finding bugs, we set the background color to something
    // obviously wrong so it will be noticable when it is not cleared
    bitmap.eraseARGB(255, 0, 255, 128);  // bright bluish green
#endif
  }
  
  CGColorSpaceRef color_space =
    CGColorSpaceCreateWithName(kCGColorSpaceGenericRGB);
  // allocate a bitmap context with 4 components per pixel (RGBA):
  CGContextRef bitmap_context =
    CGBitmapContextCreate(data, width, height, 8, width*4,
                          color_space, kCGImageAlphaPremultipliedLast);
  // change the coordinate system to match WebCore's
  CGContextTranslateCTM(bitmap_context, 0, height);
  CGContextScaleCTM(bitmap_context, 1.0, -1.0);
  CGColorSpaceRelease(color_space);

  // The device object will take ownership of the graphics context.
  return new BitmapPlatformDeviceMac(
      new BitmapPlatformDeviceMacData(bitmap_context), bitmap);
}

// The device will own the bitmap, which corresponds to also owning the pixel
// data. Therefore, we do not transfer ownership to the SkDevice's bitmap.
BitmapPlatformDeviceMac::BitmapPlatformDeviceMac(
    BitmapPlatformDeviceMacData* data, const SkBitmap& bitmap)
    : PlatformDeviceMac(bitmap),
      data_(data) {
}

// The copy constructor just adds another reference to the underlying data.
// We use a const cast since the default Skia definitions don't define the
// proper constedness that we expect (accessBitmap should really be const).
BitmapPlatformDeviceMac::BitmapPlatformDeviceMac(
    const BitmapPlatformDeviceMac& other)
    : PlatformDeviceMac(
          const_cast<BitmapPlatformDeviceMac&>(other).accessBitmap(true)),
      data_(other.data_) {
}

BitmapPlatformDeviceMac::~BitmapPlatformDeviceMac() {
}

BitmapPlatformDeviceMac& BitmapPlatformDeviceMac::operator=(
    const BitmapPlatformDeviceMac& other) {
  data_ = other.data_;
  return *this;
}

CGContextRef BitmapPlatformDeviceMac::GetBitmapContext() {
  return data_->GetBitmapContext();
}

void BitmapPlatformDeviceMac::SetTransform(const SkMatrix& matrix) {
  data_->SetTransform(matrix);
}

void BitmapPlatformDeviceMac::SetDeviceOffset(int x, int y) {
  data_->offset_x_ = x;
  data_->offset_y_ = y;
}

void BitmapPlatformDeviceMac::SetClipRegion(const SkRegion& region) {
  data_->SetClipRegion(region);
}

void BitmapPlatformDeviceMac::DrawToContext(CGContextRef context, int x, int y,
                                            const CGRect* src_rect) {
  bool created_dc = false;
  if (!data_->bitmap_context_) {
    created_dc = true;
    GetBitmapContext();
  }

  // this should not make a copy of the bits, since we're not doing
  // anything to trigger copy on write
  CGImageRef image = CGBitmapContextCreateImage(data_->bitmap_context_);
  CGRect bounds;
  if (src_rect) {
    bounds = *src_rect;
    bounds.origin.x = x;
    bounds.origin.y = y;
    CGImageRef sub_image = CGImageCreateWithImageInRect(image, *src_rect);
    CGContextDrawImage(context, bounds, sub_image);
    CGImageRelease(sub_image);
  } else {
    bounds.origin.x = 0;
    bounds.origin.y = 0;
    bounds.size.width = width();
    bounds.size.height = height();
    CGContextDrawImage(context, bounds, image);
  }
  CGImageRelease(image);

  if (created_dc)
    data_->ReleaseBitmapContext();
}

void BitmapPlatformDeviceMac::processPixels(int x, int y,
                                            int width, int height, 
                                            adjustAlpha adjustor) {
  const SkBitmap& bitmap = accessBitmap(true);
  SkMatrix& matrix = data_->transform_;
  int bitmap_start_x = SkScalarRound(matrix.getTranslateX()) + x;
  int bitmap_start_y = SkScalarRound(matrix.getTranslateY()) + y;

  SkAutoLockPixels lock(bitmap);
  if (Constrain(bitmap.width(), &bitmap_start_x, &width) &&
      Constrain(bitmap.height(), &bitmap_start_y, &height)) {
    uint32_t* data = bitmap.getAddr32(0, 0);
    size_t row_words = bitmap.rowBytes() / 4;
    for (int i = 0; i < height; i++) {
      size_t offset = (i + bitmap_start_y) * row_words + bitmap_start_x;
      for (int j = 0; j < width; j++) {
        adjustor(data + offset + j);
      }
    }
  }
}

// Returns the color value at the specified location.
SkColor BitmapPlatformDeviceMac::GetColorAt(int x, int y) {
  const SkBitmap& bitmap = accessBitmap(true);
  SkAutoLockPixels lock(bitmap);
  uint32_t* data = bitmap.getAddr32(0, 0);
  return static_cast<SkColor>(data[x + y * width()]);
}

void BitmapPlatformDeviceMac::onAccessBitmap(SkBitmap*) {
  // Not needed in CoreGraphics
}

}  // namespace gfx
