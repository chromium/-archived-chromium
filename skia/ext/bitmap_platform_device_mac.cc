// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "skia/ext/bitmap_platform_device_mac.h"

#include <time.h>

#include "base/ref_counted.h"
#include "skia/ext/skia_utils_mac.h"
#include "third_party/skia/include/core/SkMatrix.h"
#include "third_party/skia/include/core/SkRegion.h"
#include "third_party/skia/include/core/SkTypes.h"
#include "third_party/skia/include/core/SkUtils.h"

namespace skia {

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

static CGContextRef CGContextForData(void* data, int width, int height) {
  CGColorSpaceRef color_space =
      CGColorSpaceCreateWithName(kCGColorSpaceGenericRGB);
#define HAS_ARGB_SHIFTS(a, r, g, b) \
            (SK_A32_SHIFT == (a) && SK_R32_SHIFT == (r) \
             && SK_G32_SHIFT == (g) && SK_B32_SHIFT == (b))
#if defined(SK_CPU_LENDIAN) && HAS_ARGB_SHIFTS(24, 16, 8, 0)
  // Allocate a bitmap context with 4 components per pixel (BGRA).  Apple
  // recommends these flags for improved CG performance.
  CGContextRef context =
      CGBitmapContextCreate(data, width, height, 8, width * 4,
                            color_space,
                            kCGImageAlphaPremultipliedFirst |
                                kCGBitmapByteOrder32Host);
#else
#error We require that Skia's and CoreGraphics's recommended \
       image memory layout match.
#endif
#undef HAS_ARGB_SHIFTS
  CGColorSpaceRelease(color_space);

  if (!context)
    return NULL;

  // Change the coordinate system to match WebCore's
  CGContextTranslateCTM(context, 0, height);
  CGContextScaleCTM(context, 1.0, -1.0);

  return context;
}

}  // namespace

class BitmapPlatformDevice::BitmapPlatformDeviceData : public SkRefCnt {
 public:
  explicit BitmapPlatformDeviceData(CGContextRef bitmap);

  // Create/destroy CoreGraphics context for our bitmap data.
  CGContextRef GetBitmapContext() {
    LoadConfig();
    return bitmap_context_;
  }

  void ReleaseBitmapContext() {
    SkASSERT(bitmap_context_);
    CGContextRelease(bitmap_context_);
    bitmap_context_ = NULL;
  }

  // Sets the transform and clip operations. This will not update the CGContext,
  // but will mark the config as dirty. The next call of LoadConfig will
  // pick up these changes.
  void SetMatrixClip(const SkMatrix& transform, const SkRegion& region);

  // Loads the current transform and clip into the DC. Can be called even when
  // |bitmap_context_| is NULL (will be a NOP).
  void LoadConfig();

  // Lazily-created graphics context used to draw into the bitmap.
  CGContextRef bitmap_context_;

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
  friend class base::RefCounted<BitmapPlatformDeviceData>;
  ~BitmapPlatformDeviceData() {
    if (bitmap_context_)
      CGContextRelease(bitmap_context_);
  }

  // Disallow copy & assign.
  BitmapPlatformDeviceData(const BitmapPlatformDeviceData&);
  BitmapPlatformDeviceData& operator=(const BitmapPlatformDeviceData&);
};

BitmapPlatformDevice::BitmapPlatformDeviceData::BitmapPlatformDeviceData(
    CGContextRef bitmap)
    : bitmap_context_(bitmap),
      config_dirty_(true) {  // Want to load the config next time.
  SkASSERT(bitmap_context_);
  // Initialize the clip region to the entire bitmap.

  SkIRect rect;
  rect.set(0, 0,
           CGBitmapContextGetWidth(bitmap_context_),
           CGBitmapContextGetHeight(bitmap_context_));
  clip_region_ = SkRegion(rect);
  transform_.reset();
  CGContextRetain(bitmap_context_);
  // We must save the state once so that we can use the restore/save trick
  // in LoadConfig().
  CGContextSaveGState(bitmap_context_);
}

void BitmapPlatformDevice::BitmapPlatformDeviceData::SetMatrixClip(
    const SkMatrix& transform,
    const SkRegion& region) {
  transform_ = transform;
  clip_region_ = region;
  config_dirty_ = true;
}

void BitmapPlatformDevice::BitmapPlatformDeviceData::LoadConfig() {
  if (!config_dirty_ || !bitmap_context_)
    return;  // Nothing to do.
  config_dirty_ = false;

  // Transform.
  SkMatrix t(transform_);

  // We must restore and then save the state of the graphics context since the
  // calls to Load the clipping region to the context are strictly cummulative,
  // i.e., you can't replace a clip rect, other than with a save/restore.
  // But this implies that no other changes to the state are done elsewhere.
  // If we ever get to need to change this, then we must replace the clip rect
  // calls in LoadClippingRegionToCGContext() with an image mask instead.
  CGContextRestoreGState(bitmap_context_);
  CGContextSaveGState(bitmap_context_);
  LoadTransformToCGContext(bitmap_context_, t);
  t.setTranslateX(-t.getTranslateX());
  t.setTranslateY(-t.getTranslateY());
  LoadClippingRegionToCGContext(bitmap_context_, clip_region_, t);
}


// We use this static factory function instead of the regular constructor so
// that we can create the pixel data before calling the constructor. This is
// required so that we can call the base class' constructor with the pixel
// data.
BitmapPlatformDevice* BitmapPlatformDevice::Create(CGContextRef context,
                                                   int width,
                                                   int height,
                                                   bool is_opaque) {
  SkBitmap bitmap;
  bitmap.setConfig(SkBitmap::kARGB_8888_Config, width, height);
  if (bitmap.allocPixels() != true)
    return NULL;

  void* data = NULL;
  if (context) {
    data = CGBitmapContextGetData(context);
    bitmap.setPixels(data);
  } else {
    data = bitmap.getPixels();
  }

  // Note: The Windows implementation clears the Bitmap later on.
  // This bears mentioning since removal of this line makes the
  // unit tests only fail periodically (or when MallocPreScribble is set).
  bitmap.eraseARGB(0, 0, 0, 0);

  bitmap.setIsOpaque(is_opaque);

#ifndef NDEBUG
  if (is_opaque) {
    // To aid in finding bugs, we set the background color to something
    // obviously wrong so it will be noticable when it is not cleared
    bitmap.eraseARGB(255, 0, 255, 128);  // bright bluish green
  }
#endif

  if (!context)
    context = CGContextForData(data, width, height);
  else
    CGContextRetain(context);

  BitmapPlatformDevice* rv = new BitmapPlatformDevice(
      new BitmapPlatformDeviceData(context), bitmap);

  // The device object took ownership of the graphics context with its own
  // CGContextRetain call.
  CGContextRelease(context);

  return rv;
}

BitmapPlatformDevice* BitmapPlatformDevice::CreateWithData(uint8_t* data,
                                                           int width,
                                                           int height,
                                                           bool is_opaque) {
  CGContextRef context = NULL;
  if (data)
    context = CGContextForData(data, width, height);

  BitmapPlatformDevice* rv = Create(context, width, height, is_opaque);

  // The device object took ownership of the graphics context with its own
  // CGContextRetain call.
  if (context)
    CGContextRelease(context);

  return rv;
}

// The device will own the bitmap, which corresponds to also owning the pixel
// data. Therefore, we do not transfer ownership to the SkDevice's bitmap.
BitmapPlatformDevice::BitmapPlatformDevice(
    BitmapPlatformDeviceData* data, const SkBitmap& bitmap)
    : PlatformDevice(bitmap),
      data_(data) {
}

// The copy constructor just adds another reference to the underlying data.
// We use a const cast since the default Skia definitions don't define the
// proper constedness that we expect (accessBitmap should really be const).
BitmapPlatformDevice::BitmapPlatformDevice(
    const BitmapPlatformDevice& other)
    : PlatformDevice(
          const_cast<BitmapPlatformDevice&>(other).accessBitmap(true)),
      data_(other.data_) {
  data_->ref();
}

BitmapPlatformDevice::~BitmapPlatformDevice() {
  data_->unref();
}

BitmapPlatformDevice& BitmapPlatformDevice::operator=(
    const BitmapPlatformDevice& other) {
  data_ = other.data_;
  data_->ref();
  return *this;
}

CGContextRef BitmapPlatformDevice::GetBitmapContext() {
  return data_->GetBitmapContext();
}

void BitmapPlatformDevice::setMatrixClip(const SkMatrix& transform,
                                         const SkRegion& region) {
  data_->SetMatrixClip(transform, region);
}

void BitmapPlatformDevice::DrawToContext(CGContextRef context, int x, int y,
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

// Returns the color value at the specified location.
SkColor BitmapPlatformDevice::getColorAt(int x, int y) {
  const SkBitmap& bitmap = accessBitmap(true);
  SkAutoLockPixels lock(bitmap);
  uint32_t* data = bitmap.getAddr32(0, 0);
  return static_cast<SkColor>(data[x + y * width()]);
}

void BitmapPlatformDevice::onAccessBitmap(SkBitmap*) {
  // Not needed in CoreGraphics
}

void BitmapPlatformDevice::processPixels(int x, int y,
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

}  // namespace skia
