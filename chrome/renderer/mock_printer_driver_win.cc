// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/mock_printer_driver_win.h"

#include "base/gfx/gdi_util.h"
#include "base/logging.h"
#include "printing/emf_win.h"
#include "chrome/renderer/mock_printer.h"
#include "skia/ext/platform_device.h"

namespace {

// A simple class which temporarily overrides system settings.
// The bitmap image rendered via the PlayEnhMetaFile() function depends on
// some system settings.
// As a workaround for such dependency, this class saves the system settings
// and changes them. This class also restore the saved settings in its
// destructor.
class SystemSettingsOverride {
 public:
  SystemSettingsOverride() : font_smoothing_(0) {
  }

  ~SystemSettingsOverride() {
    SystemParametersInfo(SPI_SETFONTSMOOTHING, font_smoothing_, NULL, 0);
  }

  BOOL Init(BOOL font_smoothing) {
    if (!SystemParametersInfo(SPI_GETFONTSMOOTHING, 0, &font_smoothing_, 0))
      return FALSE;
    return SystemParametersInfo(SPI_SETFONTSMOOTHING, font_smoothing, NULL, 0);
  }

 private:
  BOOL font_smoothing_;
};

// A class which renders an EMF data and returns a raw bitmap data.
// The bitmap data returned from Create() is deleted in the destructor of this
// class. So, we need to create a copy of this bitmap data if it is used after
// this object is deleted,
class EmfRenderer {
 public:
  EmfRenderer() : dc_(NULL), bitmap_(NULL) {
  }

  ~EmfRenderer() {
    if (bitmap_) {
      DeleteObject(bitmap_);
      bitmap_ = NULL;
    }
    if (dc_) {
      DeleteDC(dc_);
      dc_ = NULL;
    }
  }

  const void* Create(int width, int height, const printing::Emf* emf) {
    CHECK(!dc_ && !bitmap_);

    BITMAPV4HEADER header;
    gfx::CreateBitmapV4Header(width, height, &header);

    dc_ = CreateCompatibleDC(NULL);
    if (!dc_)
      return NULL;

    void* bits;
    bitmap_ = CreateDIBSection(dc_, reinterpret_cast<BITMAPINFO*>(&header), 0,
                               &bits, NULL, 0);
    if (!bitmap_ || !bits)
      return NULL;

    SelectObject(dc_, bitmap_);

    skia::PlatformDevice::InitializeDC(dc_);
    emf->Playback(dc_, NULL);

    return reinterpret_cast<uint8*>(bits);
  }

 private:
  HDC dc_;
  HBITMAP bitmap_;
};

}  // namespace

MockPrinterDriverWin::MockPrinterDriverWin() {
}

MockPrinterDriverWin::~MockPrinterDriverWin() {
}

MockPrinterPage* MockPrinterDriverWin::LoadSource(const void* source_data,
                                                  size_t source_size) {
  // This code is mostly copied from the Image::LoadEMF() function in
  // "src/chrome/browser/printing/printing_layout_uitest.cc".
  printing::Emf emf;
  emf.CreateFromData(source_data, source_size);
  gfx::Rect rect(emf.GetBounds());

  // Create a temporary DC and bitmap to retrieve the renderered data.
  if (rect.width() <= 0 || rect.height() <= 0)
    return NULL;

  // Disable the font-smoothing feature of Windows.
  SystemSettingsOverride system_settings;
  system_settings.Init(FALSE);

  // Render the EMF data and create a bitmap.
  EmfRenderer renderer;
  const void* bitmap_data = renderer.Create(rect.width(), rect.height(), &emf);
  if (!bitmap_data)
    return NULL;

  // Create a new MockPrinterPage instance and return it.
  size_t row_byte_width = rect.width() * 4;
  size_t bitmap_size = row_byte_width * rect.height();
  return new MockPrinterPage(rect.width(), rect.height(),
                             source_data, source_size,
                             bitmap_data, bitmap_size);
}
