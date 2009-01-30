// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_host/backing_store.h"

#include "base/gfx/gdi_util.h"
#include "chrome/browser/renderer_host/render_widget_host.h"
#include "chrome/common/win_util.h"

// BackingStore (Windows) ------------------------------------------------------

BackingStore::BackingStore(const gfx::Size& size)
    : size_(size),
      backing_store_dib_(NULL),
      original_bitmap_(NULL) {
  HDC screen_dc = ::GetDC(NULL);
  hdc_ = CreateCompatibleDC(screen_dc);
  ReleaseDC(NULL, screen_dc);
}

BackingStore::~BackingStore() {
  DCHECK(hdc_);

  DeleteDC(hdc_);

  if (backing_store_dib_) {
    DeleteObject(backing_store_dib_);
    backing_store_dib_ = NULL;
  }
}

bool BackingStore::PaintRect(base::ProcessHandle process,
                             HANDLE bitmap_section,
                             const gfx::Rect& bitmap_rect) {
  // The bitmap received is valid only in the renderer process.
  HANDLE valid_bitmap =
      win_util::GetSectionFromProcess(bitmap_section, process, false);
  if (!valid_bitmap)
    return false;

  if (!backing_store_dib_) {
    backing_store_dib_ = CreateDIB(hdc_, size_.width(), size_.height(), true,
                                   NULL);
    DCHECK(backing_store_dib_ != NULL);
    original_bitmap_ = SelectObject(hdc_, backing_store_dib_);
  }

  // TODO(darin): protect against integer overflow
  DWORD size = 4 * bitmap_rect.width() * bitmap_rect.height();
  void* backing_store_data = MapViewOfFile(valid_bitmap, FILE_MAP_READ, 0, 0,
                                           size);
  // These values are shared with gfx::PlatformDevice
  BITMAPINFOHEADER hdr;
  gfx::CreateBitmapHeader(bitmap_rect.width(), bitmap_rect.height(), &hdr);
  // Account for a bitmap_rect that exceeds the bounds of our view
  gfx::Rect view_rect(0, 0, size_.width(), size_.height());
  gfx::Rect paint_rect = view_rect.Intersect(bitmap_rect);

  StretchDIBits(hdc_,
                paint_rect.x(),
                paint_rect.y(),
                paint_rect.width(),
                paint_rect.height(),
                0, 0,  // source x,y
                paint_rect.width(),
                paint_rect.height(),
                backing_store_data,
                reinterpret_cast<BITMAPINFO*>(&hdr),
                DIB_RGB_COLORS,
                SRCCOPY);

  UnmapViewOfFile(backing_store_data);
  CloseHandle(valid_bitmap);
  return true;
}

void BackingStore::ScrollRect(base::ProcessHandle process,
                              HANDLE bitmap, const gfx::Rect& bitmap_rect,
                              int dx, int dy,
                              const gfx::Rect& clip_rect,
                              const gfx::Size& view_size) {
  RECT damaged_rect, r = clip_rect.ToRECT();
  ScrollDC(hdc_, dx, dy, NULL, &r, NULL, &damaged_rect);

  // TODO(darin): this doesn't work if dx and dy are both non-zero!
  DCHECK(dx == 0 || dy == 0);

  // We expect that damaged_rect should equal bitmap_rect.
  DCHECK(gfx::Rect(damaged_rect) == bitmap_rect);

  PaintRect(process, bitmap, bitmap_rect);
}

HANDLE BackingStore::CreateDIB(HDC dc,
                               int width, int height,
                               bool use_system_color_depth,
                               HANDLE section) {
  BITMAPINFOHEADER hdr;

  if (use_system_color_depth) {
    HDC screen_dc = ::GetDC(NULL);
    int color_depth = GetDeviceCaps(screen_dc, BITSPIXEL);
    ::ReleaseDC(NULL, screen_dc);

    // Color depths less than 16 bpp require a palette to be specified in the
    // BITMAPINFO structure passed to CreateDIBSection. Instead of creating
    // the palette, we specify the desired color depth as 16 which allows the
    // OS to come up with an approximation. Tested this with 8bpp.
    if (color_depth < 16)
      color_depth = 16;

    gfx::CreateBitmapHeaderWithColorDepth(width, height, color_depth, &hdr);
  } else {
    gfx::CreateBitmapHeader(width, height, &hdr);
  }
  void* data = NULL;
  HANDLE dib =
      CreateDIBSection(hdc_, reinterpret_cast<BITMAPINFO*>(&hdr),
                       0, &data, section, 0);
  return dib;
}
