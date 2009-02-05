// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_host/backing_store.h"

#include "base/logging.h"
#include "skia/ext/platform_canvas.h"
#include "skia/include/SkBitmap.h"
#include "skia/include/SkCanvas.h"

BackingStore::BackingStore(const gfx::Size& size)
    : size_(size) {
  if (!canvas_.initialize(size.width(), size.height(), true))
    SK_CRASH();
}

BackingStore::~BackingStore() {
}

bool BackingStore::PaintRect(base::ProcessHandle process,
                             BitmapWireData bitmap,
                             const gfx::Rect& bitmap_rect) {
  if (bitmap.width() != bitmap_rect.width() ||
      bitmap.height() != bitmap_rect.height() ||
      bitmap.config() != SkBitmap::kARGB_8888_Config) {
    return false;
  }

  canvas_.drawBitmap(bitmap, bitmap_rect.x(), bitmap_rect.y());
  return true;
}

// Return the given value, clipped to 0..max (inclusive)
static int RangeClip(int value, int max) {
  return std::min(max, std::max(value, 0));
}

void BackingStore::ScrollRect(base::ProcessHandle process,
                              BitmapWireData bitmap,
                              const gfx::Rect& bitmap_rect,
                              int dx, int dy,
                              const gfx::Rect& clip_rect,
                              const gfx::Size& view_size) {
  // WARNING: this is temporary code until a real solution is found for Mac and
  // Linux.
  //
  // On Windows, there's a ScrollDC call which performs horiz and vertical
  // scrolling
  //
  // clip_rect: MSDN says "The only bits that will be painted are the
  // bits that remain inside this rectangle after the scroll operation has been
  // completed."
  //
  // The Windows code always sets the whole backing store as the source of the
  // scroll. Thus, we only have to worry about pixels which will end up inside
  // the clipping rectangle. (Note that the clipping rectangle is not
  // translated by the scrol.)

  // We only support scrolling in one direction at a time.
  DCHECK(dx == 0 || dy == 0);

  if (bitmap.width() != bitmap_rect.width() ||
      bitmap.height() != bitmap_rect.height() ||
      bitmap.config() != SkBitmap::kARGB_8888_Config) {
    return;
  }

  // We assume that |clip_rect| is within the backing store.

  const SkBitmap &backing_bitmap = canvas_.getDevice()->accessBitmap(true);
  const int stride = backing_bitmap.rowBytes();
  uint8_t* x = static_cast<uint8_t*>(backing_bitmap.getPixels());

  if (dx) {
    // Horizontal scroll. Positive values of |dx| scroll right.

    // We know that |clip_rect| is the destination rectangle. The source
    // rectangle, in an infinite world, would be the destination rectangle
    // translated by -dx. However, we can't pull pixels from beyound the
    // edge of the backing store. By truncating the source rectangle to the
    // backing store, we might have made it thinner, thus we find the final
    // dest rectangle by translating the resulting source rectangle back.
    const int bs_width = canvas_.getDevice()->width();
    const int src_rect_left = RangeClip(clip_rect.x() - dx, bs_width);
    const int src_rect_right = RangeClip(clip_rect.right() - dx, bs_width);

    const int dest_rect_left = RangeClip(src_rect_left + dx, bs_width);
    const int dest_rect_right = RangeClip(src_rect_right + dx, bs_width);

    DCHECK(dest_rect_right >= dest_rect_left);
    DCHECK(src_rect_right >= src_rect_left);
    DCHECK(dest_rect_right - dest_rect_left ==
           src_rect_right - src_rect_left);

    // This is the number of bytes to move per line at 4 bytes per pixel.
    const int len = (dest_rect_right - dest_rect_left) * 4;

    // Position the pixel data pointer at the top-left corner of the dest rect
    x += clip_rect.y() * stride;
    x += clip_rect.x() * 4;

    // This is the number of bytes to reach left in order to find the source
    // rectangle. (Will be negative for a left scroll.)
    const int left_reach = dest_rect_left - src_rect_left;

    for (int y = clip_rect.y(); y < clip_rect.bottom(); ++y) {
      // Note that overlapping regions requires memmove, not memcpy.
      memmove(x, x + left_reach, len);
      x += stride;
    }
  } else {
    // Vertical scroll. Positive values of |dy| scroll down.

    // The logic is very similar to the horizontal case, above.
    const int bs_height = canvas_.getDevice()->height();
    const int src_rect_top = RangeClip(clip_rect.y() - dy, bs_height);
    const int src_rect_bottom = RangeClip(clip_rect.bottom() - dy, bs_height);

    const int dest_rect_top = RangeClip(src_rect_top + dy, bs_height);
    const int dest_rect_bottom = RangeClip(src_rect_bottom + dy, bs_height);

    const int len = clip_rect.width() * 4;

    DCHECK(dest_rect_bottom >= dest_rect_top);
    DCHECK(src_rect_bottom >= src_rect_top);

    // We need to consider the two directions separately here because the order
    // of copying rows must vary to avoid writing data which we later need to
    // read.
    if (dy > 0) {
      // Scrolling down; dest rect is above src rect.

      // Position the pixel data pointer at the top-left corner of the dest
      // rect.
      x += dest_rect_top * stride;
      x += clip_rect.x() * 4;

      DCHECK(dest_rect_top <= src_rect_top);
      const int down_reach_bytes = stride * (src_rect_top - dest_rect_top);

      for (int y = dest_rect_top; y < dest_rect_bottom; ++y) {
        memcpy(x, x + down_reach_bytes, len);
        x += stride;
      }
    } else {
      // Scrolling up; dest rect is below src rect.

      // Position the pixel data pointer at the bottom-left corner of the dest
      // rect.
      x += (dest_rect_bottom - 1) * stride;
      x += clip_rect.x() * 4;

      DCHECK(src_rect_top <= dest_rect_top);
      const int up_reach_bytes = stride * (dest_rect_bottom - src_rect_bottom);

      for (int y = dest_rect_bottom - 1; y >= dest_rect_top; --y) {
        memcpy(x, x - up_reach_bytes, len);
        x -= stride;
      }
    }
  }

  // Now paint the new bitmap data.
  canvas_.drawBitmap(bitmap, bitmap_rect.x(), bitmap_rect.y());
  return;
}
