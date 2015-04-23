// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_host/backing_store.h"

#include "base/logging.h"
#include "chrome/common/transport_dib.h"
#include "skia/ext/platform_canvas.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkCanvas.h"

BackingStore::BackingStore(RenderWidgetHost* widget, const gfx::Size& size)
    : render_widget_host_(widget),
      size_(size) {
  if (!canvas_.initialize(size.width(), size.height(), true))
    SK_CRASH();
}

BackingStore::~BackingStore() {
}

size_t BackingStore::MemorySize() {
  // Always 4 bytes per pixel.
  return size_.GetArea() * 4;
}

void BackingStore::PaintRect(base::ProcessHandle process,
                             TransportDIB* bitmap,
                             const gfx::Rect& bitmap_rect) {
  SkBitmap skbitmap;
  skbitmap.setConfig(SkBitmap::kARGB_8888_Config, bitmap_rect.width(),
                     bitmap_rect.height(), 4 * bitmap_rect.width());

  skbitmap.setPixels(bitmap->memory());

  canvas_.drawBitmap(skbitmap, bitmap_rect.x(), bitmap_rect.y());
}

void BackingStore::ScrollRect(base::ProcessHandle process,
                              TransportDIB* bitmap,
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
  // translated by the scroll.)

  // We only support scrolling in one direction at a time.
  DCHECK(dx == 0 || dy == 0);

  // We assume |clip_rect| is contained within the backing store.
  DCHECK(clip_rect.bottom() <= canvas_.getDevice()->height());
  DCHECK(clip_rect.right() <= canvas_.getDevice()->width());

  const SkBitmap &backing_bitmap = canvas_.getDevice()->accessBitmap(true);
  const int stride = backing_bitmap.rowBytes();
  uint8_t* x = static_cast<uint8_t*>(backing_bitmap.getPixels());

  if (dx) {
    // Do not memmove any data if the scroll distance (|dx|) is greater
    // than |clip_rect.width()|.  If this is true, then none of the
    // previous pixels will still be on the screen after the scroll.
    // The call to PaintRect() below will redraw the entire area.
    if (abs(dx) < clip_rect.width()) {
      // Horizontal scroll. According to msdn, positive values of |dx| scroll
      // left, but in practice this seems reversed. TODO(port): figure this
      // out. For now just reverse the sign.
      dx *= -1;

      // This is the number of bytes to move per line at 4 bytes per pixel.
      const int len = (clip_rect.width() - abs(dx)) * 4;

      // Move |x| to the first pixel of the first row.
      x += clip_rect.y() * stride;
      x += clip_rect.x() * 4;

      // If we are scrolling left, move |x| to the |dx|^th pixel.
      if (dx < 0) {
        x -= dx * 4;
      }

      for (int i = clip_rect.y(); i < clip_rect.bottom(); ++i) {
        // Note that overlapping regions requires memmove, not memcpy.
        memmove(x, x + dx * 4, len);
        x += stride;
      }
    }
  } else if (dy) {
    // Vertical scroll.  The above warning about not copying data if
    // |dy| > |clip_rect.height()| technically applies here too, but
    // is implicitly taken care of by the termination condition in the
    // for loop, so we do not need to explicitly check against |dy|.

    // TODO(port): According to msdn, positive values of |dy| scroll
    // down, but in practice this seems reversed.  Figure this
    // out. For now just reverse the sign.
    dy *= -1;

    const int len = clip_rect.width() * 4;

    // For down scrolls, we copy bottom-up (in screen coordinates).
    // For up scrolls, we copy top-down.
    if (dy > 0) {
      // Move |x| to the first pixel of the first row of the clip rect.
      x += clip_rect.y() * stride;
      x += clip_rect.x() * 4;

      for (int i = 0; i < clip_rect.height() - dy; ++i) {
        memcpy(x, x + stride * dy, len);
        x += stride;
      }
    } else {
      // Move |x| to the first pixel of the last row of the clip rect.
      x += (clip_rect.bottom() - 1) * stride;
      x += clip_rect.x() * 4;

      for (int i = 0; i < clip_rect.height() + dy; ++i) {
        memcpy(x, x + stride * dy, len);
        x -= stride;
      }
    }
  }

  // Now paint the new bitmap data.
  PaintRect(process, bitmap, bitmap_rect);
  return;
}
