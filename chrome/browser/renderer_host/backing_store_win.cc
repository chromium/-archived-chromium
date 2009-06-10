// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_host/backing_store.h"

#include "base/gfx/gdi_util.h"
#include "chrome/browser/renderer_host/render_widget_host.h"
#include "chrome/common/transport_dib.h"

namespace {

// Creates a dib conforming to the height/width/section parameters passed in.
HANDLE CreateDIB(HDC dc, int width, int height, int color_depth) {
  BITMAPINFOHEADER hdr;
  gfx::CreateBitmapHeaderWithColorDepth(width, height, color_depth, &hdr);
  void* data = NULL;
  HANDLE dib = CreateDIBSection(dc, reinterpret_cast<BITMAPINFO*>(&hdr),
                                0, &data, NULL, 0);
  DCHECK(data);
  return dib;
}

}  // namespace

// BackingStore (Windows) ------------------------------------------------------

BackingStore::BackingStore(const gfx::Size& size)
    : size_(size),
      backing_store_dib_(NULL),
      original_bitmap_(NULL) {
  HDC screen_dc = ::GetDC(NULL);
  color_depth_ = ::GetDeviceCaps(screen_dc, BITSPIXEL);
  // Color depths less than 16 bpp require a palette to be specified. Instead,
  // we specify the desired color depth as 16 which lets the OS to come up
  // with an approximation.
  if (color_depth_ < 16)
    color_depth_ = 16;
  hdc_ = CreateCompatibleDC(screen_dc);
  ReleaseDC(NULL, screen_dc);
}

BackingStore::~BackingStore() {
  DCHECK(hdc_);
  if (original_bitmap_) {
    SelectObject(hdc_, original_bitmap_);
  }
  if (backing_store_dib_) {
    DeleteObject(backing_store_dib_);
    backing_store_dib_ = NULL;
  }
  DeleteDC(hdc_);
}

void BackingStore::PaintRect(base::ProcessHandle process,
                             TransportDIB* bitmap,
                             const gfx::Rect& bitmap_rect,
                             const gfx::Rect& paint_rect) {
  DCHECK(bitmap_rect.Contains(paint_rect) &&
         paint_rect.x() < kMaxBitmapLengthAllowed &&
         paint_rect.y() < kMaxBitmapLengthAllowed);
  if (!backing_store_dib_) {
    backing_store_dib_ = CreateDIB(hdc_, size_.width(),
                                   size_.height(), color_depth_);
    if (!backing_store_dib_) {
      NOTREACHED();
      return;
    }
    original_bitmap_ = SelectObject(hdc_, backing_store_dib_);
  }

  BITMAPINFOHEADER hdr;
  gfx::CreateBitmapHeader(bitmap_rect.width(), bitmap_rect.height(), &hdr);
  // Account for a paint_rect that exceeds the bounds of our view.
  gfx::Rect view_rect(0, 0, size_.width(), size_.height());
  gfx::Rect paint_view_rect = view_rect.Intersect(paint_rect);

  // CreateBitmapHeader specifies a negative height,
  // so the y destination is from the bottom.
  int source_x = paint_view_rect.x() - bitmap_rect.x();
  int source_y = bitmap_rect.bottom() - paint_view_rect.bottom();
  int source_height = paint_view_rect.height();
  int destination_y = paint_view_rect.y();
  int destination_height = paint_view_rect.height();
  if (source_y == 0 && source_x == 0 &&
      paint_view_rect.height() != bitmap_rect.height()) {
    // StretchDIBits has a bug where it won't take the proper source
    // rect if it starts at (0, 0) in the source but not in the destination,
    // so we must use a mirror blit trick as proposed here:
    // http://wiki.allegro.cc/index.php?title=StretchDIBits
    destination_y += destination_height - 1;
    destination_height = -destination_height;
    source_y = bitmap_rect.height() - paint_view_rect.y() + 1;
    source_height = -source_height;
  }

  int rv = StretchDIBits(hdc_, paint_view_rect.x(), destination_y,
                         paint_view_rect.width(), destination_height,
                         source_x, source_y, paint_view_rect.width(),
                         source_height, bitmap->memory(),
                         reinterpret_cast<BITMAPINFO*>(&hdr),
                         DIB_RGB_COLORS, SRCCOPY);
  DCHECK(rv != GDI_ERROR);
}

void BackingStore::ScrollRect(base::ProcessHandle process,
                              TransportDIB* bitmap,
                              const gfx::Rect& bitmap_rect,
                              int dx, int dy,
                              const gfx::Rect& clip_rect,
                              const gfx::Size& view_size) {
  RECT damaged_rect, r = clip_rect.ToRECT();
  ScrollDC(hdc_, dx, dy, NULL, &r, NULL, &damaged_rect);

  // TODO(darin): this doesn't work if dx and dy are both non-zero!
  DCHECK(dx == 0 || dy == 0);

  // We expect that damaged_rect should equal bitmap_rect.
  DCHECK(gfx::Rect(damaged_rect) == bitmap_rect);

  PaintRect(process, bitmap, bitmap_rect, bitmap_rect);
}
