// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_host/backing_store.h"

#include "base/command_line.h"
#include "base/gfx/gdi_util.h"
#include "chrome/browser/renderer_host/render_widget_host.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/transport_dib.h"

namespace {

// Creates a dib conforming to the height/width/section parameters passed in.
HANDLE CreateDIB(HDC dc, int width, int height, int color_depth) {
  BITMAPV5HEADER hdr = {0};
  ZeroMemory(&hdr, sizeof(BITMAPV5HEADER));

  // These values are shared with gfx::PlatformDevice
  hdr.bV5Size = sizeof(BITMAPINFOHEADER);
  hdr.bV5Width = width;
  hdr.bV5Height = -height;  // minus means top-down bitmap
  hdr.bV5Planes = 1;
  hdr.bV5BitCount = color_depth;
  hdr.bV5Compression = BI_RGB;  // no compression
  hdr.bV5SizeImage = 0;
  hdr.bV5XPelsPerMeter = 1;
  hdr.bV5YPelsPerMeter = 1;
  hdr.bV5ClrUsed = 0;
  hdr.bV5ClrImportant = 0;

  if (BackingStore::ColorManagementEnabled()) {
    hdr.bV5CSType = LCS_sRGB;
    hdr.bV5Intent = LCS_GM_IMAGES;
  }

  void* data = NULL;
  HANDLE dib = CreateDIBSection(dc, reinterpret_cast<BITMAPINFO*>(&hdr),
                                0, &data, NULL, 0);
  DCHECK(data);
  return dib;
}

}  // namespace

// BackingStore (Windows) ------------------------------------------------------

BackingStore::BackingStore(RenderWidgetHost* widget, const gfx::Size& size)
    : render_widget_host_(widget),
      size_(size),
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

size_t BackingStore::MemorySize() {
  return size_.GetArea() * (color_depth_ / 8);
}

// static
bool BackingStore::ColorManagementEnabled() {
  static bool enabled = false;
  static bool checked = false;
  if (!checked) {
    checked = true;
    const CommandLine& command = *CommandLine::ForCurrentProcess();
    enabled = command.HasSwitch(switches::kEnableMonitorProfile);
  }
  return enabled;
}

void BackingStore::PaintRect(base::ProcessHandle process,
                             TransportDIB* bitmap,
                             const gfx::Rect& bitmap_rect) {
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
  // Account for a bitmap_rect that exceeds the bounds of our view
  gfx::Rect view_rect(0, 0, size_.width(), size_.height());
  gfx::Rect paint_rect = view_rect.Intersect(bitmap_rect);

  int rv = StretchDIBits(hdc_,
                         paint_rect.x(), paint_rect.y(),
                         paint_rect.width(), paint_rect.height(),
                         0, 0,  // source x,y.
                         paint_rect.width(), paint_rect.height(),
                         bitmap->memory(),
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

  PaintRect(process, bitmap, bitmap_rect);
}
