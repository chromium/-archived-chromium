// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "skia/ext/bitmap_platform_device_win.h"

#include "SkMatrix.h"
#include "SkRefCnt.h"
#include "SkRegion.h"
#include "SkUtils.h"

namespace skia {

namespace {

// Constrains position and size to fit within available_size. If |size| is -1,
// all the available_size is used. Returns false if the position is out of
// available_size.
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

}  // namespace

class BitmapPlatformDeviceWin::BitmapPlatformDeviceWinData : public SkRefCnt {
 public:
  explicit BitmapPlatformDeviceWinData(HBITMAP hbitmap);

  // Create/destroy hdc_, which is the memory DC for our bitmap data.
  HDC GetBitmapDC();
  void ReleaseBitmapDC();
  bool IsBitmapDCCreated() const;

  // Sets the transform and clip operations. This will not update the DC,
  // but will mark the config as dirty. The next call of LoadConfig will
  // pick up these changes.
  void SetMatrixClip(const SkMatrix& transform, const SkRegion& region);

  const SkMatrix& transform() const {
    return transform_;
  }

 protected:
  // Loads the current transform and clip into the DC. Can be called even when
  // the DC is NULL (will be a NOP).
  void LoadConfig();

  // Windows bitmap corresponding to our surface.
  HBITMAP hbitmap_;

  // Lazily-created DC used to draw into the bitmap, see getBitmapDC.
  HDC hdc_;

  // True when there is a transform or clip that has not been set to the DC.
  // The DC is retrieved for every text operation, and the transform and clip
  // do not change as much. We can save time by not loading the clip and
  // transform for every one.
  bool config_dirty_;

  // Translation assigned to the DC: we need to keep track of this separately
  // so it can be updated even if the DC isn't created yet.
  SkMatrix transform_;

  // The current clipping
  SkRegion clip_region_;

 private:
  virtual ~BitmapPlatformDeviceWinData();

  // Copy & assign are not supported.
  BitmapPlatformDeviceWinData(const BitmapPlatformDeviceWinData&);
  BitmapPlatformDeviceWinData& operator=(const BitmapPlatformDeviceWinData&);
};

BitmapPlatformDeviceWin::BitmapPlatformDeviceWinData::BitmapPlatformDeviceWinData(
    HBITMAP hbitmap)
    : hbitmap_(hbitmap),
      hdc_(NULL),
      config_dirty_(true) {  // Want to load the config next time.
  // Initialize the clip region to the entire bitmap.
  BITMAP bitmap_data;
  if (GetObject(hbitmap_, sizeof(BITMAP), &bitmap_data)) {
    SkIRect rect;
    rect.set(0, 0, bitmap_data.bmWidth, bitmap_data.bmHeight);
    clip_region_ = SkRegion(rect);
  }

  transform_.reset();
}

BitmapPlatformDeviceWin::BitmapPlatformDeviceWinData::~BitmapPlatformDeviceWinData() {
  if (hdc_)
    ReleaseBitmapDC();

  // this will free the bitmap data as well as the bitmap handle
  DeleteObject(hbitmap_);
}

HDC BitmapPlatformDeviceWin::BitmapPlatformDeviceWinData::GetBitmapDC() {
  if (!hdc_) {
    hdc_ = CreateCompatibleDC(NULL);
    InitializeDC(hdc_);
    HGDIOBJ old_bitmap = SelectObject(hdc_, hbitmap_);
    // When the memory DC is created, its display surface is exactly one
    // monochrome pixel wide and one monochrome pixel high. Since we select our
    // own bitmap, we must delete the previous one.
    DeleteObject(old_bitmap);
  }

  LoadConfig();
  return hdc_;
}

void BitmapPlatformDeviceWin::BitmapPlatformDeviceWinData::ReleaseBitmapDC() {
  SkASSERT(hdc_);
  DeleteDC(hdc_);
  hdc_ = NULL;
}

bool BitmapPlatformDeviceWin::BitmapPlatformDeviceWinData::IsBitmapDCCreated()
    const {
  return hdc_ != NULL;
}


void BitmapPlatformDeviceWin::BitmapPlatformDeviceWinData::SetMatrixClip(
    const SkMatrix& transform,
    const SkRegion& region) {
  transform_ = transform;
  clip_region_ = region;
  config_dirty_ = true;
}

void BitmapPlatformDeviceWin::BitmapPlatformDeviceWinData::LoadConfig() {
  if (!config_dirty_ || !hdc_)
    return;  // Nothing to do.
  config_dirty_ = false;

  // Transform.
  SkMatrix t(transform_);
  LoadTransformToDC(hdc_, t);
  // We don't use transform_ for the clipping region since the translation is
  // already applied to offset_x_ and offset_y_.
  t.reset();
  LoadClippingRegionToDC(hdc_, clip_region_, t);
}

// We use this static factory function instead of the regular constructor so
// that we can create the pixel data before calling the constructor. This is
// required so that we can call the base class' constructor with the pixel
// data.
BitmapPlatformDeviceWin* BitmapPlatformDeviceWin::create(
    HDC screen_dc,
    int width,
    int height,
    bool is_opaque,
    HANDLE shared_section) {
  SkBitmap bitmap;

  // CreateDIBSection appears to get unhappy if we create an empty bitmap, so
  // just create a minimal bitmap
  if ((width == 0) || (height == 0)) {
    width = 1;
    height = 1;
  }

  BITMAPINFOHEADER hdr = {0};
  hdr.biSize = sizeof(BITMAPINFOHEADER);
  hdr.biWidth = width;
  hdr.biHeight = -height;  // minus means top-down bitmap
  hdr.biPlanes = 1;
  hdr.biBitCount = 32;
  hdr.biCompression = BI_RGB;  // no compression
  hdr.biSizeImage = 0;
  hdr.biXPelsPerMeter = 1;
  hdr.biYPelsPerMeter = 1;
  hdr.biClrUsed = 0;
  hdr.biClrImportant = 0;

  void* data = NULL;
  HBITMAP hbitmap = CreateDIBSection(screen_dc,
                                     reinterpret_cast<BITMAPINFO*>(&hdr), 0,
                                     &data,
                                     shared_section, 0);

  // If we run out of GDI objects or some other error occurs, we won't get a
  // bitmap here. This will cause us to crash later because the data pointer is
  // NULL. To make sure that we can assign blame for those crashes to this code,
  // we deliberately crash here, even in release mode.
  if (!hbitmap) {
    DWORD error = GetLastError();
    return NULL;
  }

  bitmap.setConfig(SkBitmap::kARGB_8888_Config, width, height);
  bitmap.setPixels(data);
  bitmap.setIsOpaque(is_opaque);

  if (is_opaque) {
#ifndef NDEBUG
    // To aid in finding bugs, we set the background color to something
    // obviously wrong so it will be noticable when it is not cleared
    bitmap.eraseARGB(255, 0, 255, 128);  // bright bluish green
#endif
  } else {
    bitmap.eraseARGB(0, 0, 0, 0);
  }

  // The device object will take ownership of the HBITMAP. The initial refcount
  // of the data object will be 1, which is what the constructor expects.
  return new BitmapPlatformDeviceWin(new BitmapPlatformDeviceWinData(hbitmap),
                                     bitmap);
}

// The device will own the HBITMAP, which corresponds to also owning the pixel
// data. Therefore, we do not transfer ownership to the SkDevice's bitmap.
BitmapPlatformDeviceWin::BitmapPlatformDeviceWin(
    BitmapPlatformDeviceWinData* data,
    const SkBitmap& bitmap)
    : PlatformDeviceWin(bitmap),
      data_(data) {
  // The data object is already ref'ed for us by create().
}

// The copy constructor just adds another reference to the underlying data.
// We use a const cast since the default Skia definitions don't define the
// proper constedness that we expect (accessBitmap should really be const).
BitmapPlatformDeviceWin::BitmapPlatformDeviceWin(
    const BitmapPlatformDeviceWin& other)
    : PlatformDeviceWin(
          const_cast<BitmapPlatformDeviceWin&>(other).accessBitmap(true)),
      data_(other.data_) {
  data_->ref();
}

BitmapPlatformDeviceWin::~BitmapPlatformDeviceWin() {
  data_->unref();
}

BitmapPlatformDeviceWin& BitmapPlatformDeviceWin::operator=(
    const BitmapPlatformDeviceWin& other) {
  data_ = other.data_;
  data_->ref();
  return *this;
}

HDC BitmapPlatformDeviceWin::getBitmapDC() {
  return data_->GetBitmapDC();
}

void BitmapPlatformDeviceWin::setMatrixClip(const SkMatrix& transform,
                                            const SkRegion& region) {
  data_->SetMatrixClip(transform, region);
}

void BitmapPlatformDeviceWin::drawToHDC(HDC dc, int x, int y,
                                        const RECT* src_rect) {
  bool created_dc = !data_->IsBitmapDCCreated();
  HDC source_dc = getBitmapDC();

  RECT temp_rect;
  if (!src_rect) {
    temp_rect.left = 0;
    temp_rect.right = width();
    temp_rect.top = 0;
    temp_rect.bottom = height();
    src_rect = &temp_rect;
  }

  int copy_width = src_rect->right - src_rect->left;
  int copy_height = src_rect->bottom - src_rect->top;

  // We need to reset the translation for our bitmap or (0,0) won't be in the
  // upper left anymore
  SkMatrix identity;
  identity.reset();

  LoadTransformToDC(source_dc, identity);
  if (isOpaque()) {
    BitBlt(dc,
           x,
           y,
           copy_width,
           copy_height,
           source_dc,
           src_rect->left,
           src_rect->top,
           SRCCOPY);
  } else {
    SkASSERT(copy_width != 0 && copy_height != 0);
    BLENDFUNCTION blend_function = {AC_SRC_OVER, 0, 255, AC_SRC_ALPHA};
    GdiAlphaBlend(dc,
                  x,
                  y,
                  copy_width,
                  copy_height,
                  source_dc,
                  src_rect->left,
                  src_rect->top,
                  copy_width,
                  copy_height,
                  blend_function);
  }
  LoadTransformToDC(source_dc, data_->transform());

  if (created_dc)
    data_->ReleaseBitmapDC();
}

void BitmapPlatformDeviceWin::makeOpaque(int x, int y, int width, int height) {
  const SkBitmap& bitmap = accessBitmap(true);
  SkASSERT(bitmap.config() == SkBitmap::kARGB_8888_Config);

  // FIXME(brettw): This is kind of lame, we shouldn't be dealing with
  // transforms at this level. Probably there should be a PlatformCanvas
  // function that does the transform (using the actual transform not just the
  // translation) and calls us with the transformed rect.
  const SkMatrix& matrix = data_->transform();
  int bitmap_start_x = SkScalarRound(matrix.getTranslateX()) + x;
  int bitmap_start_y = SkScalarRound(matrix.getTranslateY()) + y;

  if (Constrain(bitmap.width(), &bitmap_start_x, &width) &&
      Constrain(bitmap.height(), &bitmap_start_y, &height)) {
    SkAutoLockPixels lock(bitmap);
    SkASSERT(bitmap.rowBytes() % sizeof(uint32_t) == 0u);
    size_t row_words = bitmap.rowBytes() / sizeof(uint32_t);
    // Set data to the first pixel to be modified.
    uint32_t* data = bitmap.getAddr32(0, 0) + (bitmap_start_y * row_words) +
                     bitmap_start_x;
    for (int i = 0; i < height; i++) {
      for (int j = 0; j < width; j++)
        data[j] |= (0xFF << SK_A32_SHIFT);
      data += row_words;
    }
  }
}

// Returns the color value at the specified location.
SkColor BitmapPlatformDeviceWin::getColorAt(int x, int y) {
  const SkBitmap& bitmap = accessBitmap(false);
  SkAutoLockPixels lock(bitmap);
  uint32_t* data = bitmap.getAddr32(0, 0);
  return static_cast<SkColor>(data[x + y * width()]);
}

void BitmapPlatformDeviceWin::onAccessBitmap(SkBitmap* bitmap) {
  // FIXME(brettw) OPTIMIZATION: We should only flush if we know a GDI
  // operation has occurred on our DC.
  if (data_->IsBitmapDCCreated())
    GdiFlush();
}

}  // namespace skia

