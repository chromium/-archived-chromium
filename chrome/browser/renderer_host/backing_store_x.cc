// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_host/backing_store.h"

#include <stdlib.h>
#include <utility>

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "chrome/common/transport_dib.h"
#include "chrome/common/x11_util.h"
#include "chrome/common/x11_util_internal.h"

// X Backing Stores:
//
// Unlike Windows, where the backing store is kept in heap memory, we keep our
// backing store in the X server, as a pixmap. Thus expose events just require
// instructing the X server to copy from the backing store to the window.
//
// The backing store is in the same format as the visual which our main window
// is using. Bitmaps from the renderer are uploaded to the X server, either via
// shared memory or over the wire, and XRENDER is used to convert them to the
// correct format for the backing store.

BackingStore::BackingStore(const gfx::Size& size,
                           Display* display,
                           int depth,
                           void* visual,
                           Drawable root_window,
                           bool use_render,
                           bool use_shared_memory)
    : size_(size),
      display_(display),
      use_shared_memory_(use_shared_memory),
      use_render_(use_render),
      visual_depth_(depth),
      root_window_(root_window) {
  const int width = size.width();
  const int height = size.height();

  COMPILE_ASSERT(__BYTE_ORDER == __LITTLE_ENDIAN, assumes_little_endian);

  pixmap_ = XCreatePixmap(display_, root_window, width, height, depth);

  if (use_render_) {
    picture_ = XRenderCreatePicture(
        display_, pixmap_,
        x11_util::GetRenderVisualFormat(display_, static_cast<Visual*>(visual)),
        0, NULL);
  } else {
    picture_ = 0;
    pixmap_bpp_ = x11_util::BitsPerPixelForPixmapDepth(display, depth);
  }

  pixmap_gc_ = XCreateGC(display_, pixmap_, 0, NULL);
}

BackingStore::BackingStore(const gfx::Size& size)
    : size_(size),
      display_(NULL),
      use_shared_memory_(false),
      use_render_(false),
      visual_depth_(-1),
      root_window_(0) {
}

BackingStore::~BackingStore() {
  // In unit tests, display_ may be NULL.
  if (!display_)
    return;

  XRenderFreePicture(display_, picture_);
  XFreePixmap(display_, pixmap_);
  XFreeGC(display_, static_cast<GC>(pixmap_gc_));
}

void BackingStore::PaintRectWithoutXrender(TransportDIB* bitmap,
                                           const gfx::Rect& bitmap_rect,
                                           const gfx::Rect& paint_rect) {
  const int paint_width = paint_rect.width();
  const int paint_height = paint_rect.height();
  Pixmap pixmap = XCreatePixmap(display_, root_window_, paint_width,
                                paint_height, visual_depth_);

  const int bitmap_width = bitmap_rect.width();
  const int bitmap_height = bitmap_rect.height();
  XImage image;
  memset(&image, 0, sizeof(image));

  image.width = bitmap_width;
  image.height = bitmap_height;
  image.format = ZPixmap;
  image.byte_order = LSBFirst;
  image.bitmap_unit = 8;
  image.bitmap_bit_order = LSBFirst;
  image.red_mask = 0xff;
  image.green_mask = 0xff00;
  image.blue_mask = 0xff0000;

  const int x_offset = paint_rect.x() - bitmap_rect.x();
  const int y_offset = paint_rect.y() - bitmap_rect.y();
  if (pixmap_bpp_ == 32) {
    // If the X server depth is already 32-bits, then our job is easy.
    image.depth = visual_depth_;
    image.bits_per_pixel = 32;
    image.bytes_per_line = bitmap_width * 4;
    image.data = static_cast<char*>(bitmap->memory());

    XPutImage(display_, pixmap, static_cast<GC>(pixmap_gc_), &image,
              x_offset, y_offset /* source x, y */, 0, 0 /* dest x, y */,
              paint_width, paint_height);
  } else if (pixmap_bpp_ == 24) {
    // In this case we just need to strip the alpha channel out of each
    // pixel. This is the case which covers VNC servers since they don't
    // support Xrender but typically have 24-bit visuals.
    //
    // It's possible to use some fancy SSE tricks here, but since this is the
    // slow path anyway, we do it slowly.

    uint8_t* bitmap24 = static_cast<uint8_t*>(malloc(3 * paint_width *
                                                     paint_height));
    if (!bitmap24)
      return;
    const uint32_t* bitmap_in = static_cast<const uint32_t*>(bitmap->memory());
    const int x_limit = paint_rect.right() - bitmap_rect.x();
    const int y_limit = paint_rect.bottom() - bitmap_rect.y();
    const int row_length = bitmap_width;
    bitmap_in += row_length * y_offset;
    for (int y = y_offset; y < y_limit; ++y) {
      bitmap_in += x_offset;
      for (int x = x_offset; x < x_limit; ++x) {
        const uint32_t pixel = *(bitmap_in++);
        bitmap24[0] = (pixel >> 16) & 0xff;
        bitmap24[1] = (pixel >> 8) & 0xff;
        bitmap24[2] = pixel & 0xff;
        bitmap24 += 3;
      }
      bitmap_in += row_length - x_limit;
    }

    image.width = paint_width;
    image.height = paint_height;
    image.depth = visual_depth_;
    image.bits_per_pixel = 24;
    image.bytes_per_line = paint_width * 3;
    image.data = reinterpret_cast<char*>(bitmap24);

    XPutImage(display_, pixmap, static_cast<GC>(pixmap_gc_), &image,
              0, 0 /* source x, y */, 0, 0 /* dest x, y */,
              paint_width, paint_height);

    free(bitmap24);
  } else if (pixmap_bpp_ == 16) {
    // Some folks have VNC setups which still use 16-bit visuals and VNC
    // doesn't include Xrender.

    uint16_t* bitmap16 = static_cast<uint16_t*>(malloc(2 * paint_width *
                                                       paint_height));
    if (!bitmap16)
      return;
    uint16_t* const orig_bitmap16 = bitmap16;
    const uint32_t* bitmap_in = static_cast<const uint32_t*>(bitmap->memory());
    const int x_limit = paint_rect.right() - bitmap_rect.x();
    const int y_limit = paint_rect.bottom() - bitmap_rect.y();
    const int row_length = bitmap_width;
    bitmap_in += row_length * y_offset;
    for (int y = y_offset; y < y_limit; ++y) {
      bitmap_in += x_offset;
      for (int x = x_offset; x < x_limit; ++x) {
        const uint32_t pixel = *(bitmap_in++);
        uint16_t out_pixel = ((pixel >> 8) & 0xf800) |
                             ((pixel >> 5) & 0x07e0) |
                             ((pixel >> 3) & 0x001f);
        *(bitmap16++) = out_pixel;
      }
      bitmap_in += row_length - x_limit;
    }

    image.depth = visual_depth_;
    image.bits_per_pixel = 16;
    image.bytes_per_line = paint_width * 2;
    image.data = reinterpret_cast<char*>(orig_bitmap16);

    image.red_mask = 0xf800;
    image.green_mask = 0x07e0;
    image.blue_mask = 0x001f;

    XPutImage(display_, pixmap, static_cast<GC>(pixmap_gc_), &image,
              0, 0 /* source x, y */, 0, 0 /* dest x, y */,
              paint_width, paint_height);
    free(orig_bitmap16);
  } else {
    CHECK(false) << "Sorry, we don't support your visual depth without "
                    "Xrender support (depth:" << visual_depth_
                 << " bpp:" << pixmap_bpp_ << ")";
  }

  XCopyArea(display_, pixmap /* source */, pixmap_ /* target */,
            static_cast<GC>(pixmap_gc_),
            0, 0 /* source x, y */, paint_width, paint_height,
            paint_rect.x(), paint_rect.y() /* dest x, y */);

  XFreePixmap(display_, pixmap);
}

void BackingStore::PaintRect(base::ProcessHandle process,
                             TransportDIB* bitmap,
                             const gfx::Rect& bitmap_rect,
                             const gfx::Rect& paint_rect) {
  DCHECK(bitmap_rect.Contains(paint_rect));
  if (!display_)
    return;

  if (bitmap_rect.IsEmpty())
    return;

  const int paint_width = paint_rect.width();
  const int paint_height = paint_rect.height();

  // For the cases where we only use the size of the paint_rect for the bitmap
  // we start at (0,0) but we use a local var to allow setting elsewhere
  // if we get to use the whole bitmap_rect size for the bitmap.
  int src_x = 0;
  int src_y = 0;
  if (paint_width > kMaxBitmapLengthAllowed ||
      paint_width > kMaxBitmapLengthAllowed)
    return;

  if (!use_render_)
    return PaintRectWithoutXrender(bitmap, bitmap_rect, paint_rect);

  Picture picture;
  Pixmap pixmap;

  if (use_shared_memory_) {
    const XID shmseg = bitmap->MapToX(display_);

    XShmSegmentInfo shminfo;
    memset(&shminfo, 0, sizeof(shminfo));
    shminfo.shmseg = shmseg;

    // The NULL in the following is the |data| pointer: this is an artifact of
    // Xlib trying to be helpful, rather than just exposing the X protocol. It
    // assumes that we have the shared memory segment mapped into our memory,
    // which we don't, and it's trying to calculate an offset by taking the
    // difference between the |data| pointer and the address of the mapping in
    // |shminfo|. Since both are NULL, the offset will be calculated to be 0,
    // which is correct for us.
    pixmap = XShmCreatePixmap(display_, root_window_, NULL, &shminfo,
                              bitmap_rect.width(), bitmap_rect.height(), 32);

    // Since we use the whole source bitmap, we must offset the source.
    src_x = paint_rect.x() - bitmap_rect.x();
    src_y = paint_rect.y() - bitmap_rect.y();
  } else {
    // No shared memory support, we have to copy the bitmap contents to the X
    // server. Xlib wraps the underlying PutImage call behind several layers of
    // functions which try to convert the image into the format which the X
    // server expects. The following values hopefully disable all conversions.
    XImage image;
    memset(&image, 0, sizeof(image));

    image.width = paint_width;
    image.height = paint_height;
    image.depth = 32;
    image.bits_per_pixel = 32;
    image.format = ZPixmap;
    image.byte_order = LSBFirst;
    image.bitmap_unit = 8;
    image.bitmap_bit_order = LSBFirst;
    image.bytes_per_line = paint_width * 4;
    image.red_mask = 0xff;
    image.green_mask = 0xff00;
    image.blue_mask = 0xff0000;
    // TODO(agl): check if we can make this more efficient.
    image.data = static_cast<char*>(bitmap->memory());

    pixmap = XCreatePixmap(display_, root_window_, paint_width, paint_height,
                           32);
    GC gc = XCreateGC(display_, pixmap, 0, NULL);
    XPutImage(display_, pixmap, gc, &image, paint_rect.x() - bitmap_rect.x(),
              paint_rect.y() - bitmap_rect.y() /* source x, y */,
               0, 0 /* dest x, y */, paint_width, paint_height);
    XFreeGC(display_, gc);
  }

  picture = x11_util::CreatePictureFromSkiaPixmap(display_, pixmap);
  XRenderComposite(display_, PictOpSrc, picture /* source */, 0 /* mask */,
                   picture_ /* dest */, src_x, src_y /* source x, y */,
                   0, 0 /* mask x, y */,
                   paint_rect.x(), paint_rect.y() /* target x, y */,
                   paint_width, paint_height);

  // In the case of shared memory, we wait for the composite to complete so that
  // we are sure that the X server has finished reading.
  if (use_shared_memory_)
    XSync(display_, False);

  XRenderFreePicture(display_, picture);
  XFreePixmap(display_, pixmap);
}

void BackingStore::ScrollRect(base::ProcessHandle process,
                              TransportDIB* bitmap,
                              const gfx::Rect& bitmap_rect,
                              int dx, int dy,
                              const gfx::Rect& clip_rect,
                              const gfx::Size& view_size) {
  if (!display_)
    return;

  // We only support scrolling in one direction at a time.
  DCHECK(dx == 0 || dy == 0);

  if (dy) {
    // Positive values of |dy| scroll up
    if (abs(dy) < clip_rect.height()) {
      XCopyArea(display_, pixmap_, pixmap_, static_cast<GC>(pixmap_gc_),
                clip_rect.x() /* source x */,
                std::max(clip_rect.y(), clip_rect.y() - dy),
                clip_rect.width(),
                clip_rect.height() - abs(dy),
                clip_rect.x() /* dest x */,
                std::max(clip_rect.y(), clip_rect.y() + dy) /* dest y */);
    }
  } else if (dx) {
    // Positive values of |dx| scroll right
    if (abs(dx) < clip_rect.width()) {
      XCopyArea(display_, pixmap_, pixmap_, static_cast<GC>(pixmap_gc_),
                std::max(clip_rect.x(), clip_rect.x() - dx),
                clip_rect.y() /* source y */,
                clip_rect.width() - abs(dx),
                clip_rect.height(),
                std::max(clip_rect.x(), clip_rect.x() + dx) /* dest x */,
                clip_rect.y() /* dest x */);
    }
  }

  PaintRect(process, bitmap, bitmap_rect, bitmap_rect);
}

void BackingStore::ShowRect(const gfx::Rect& rect, XID target) {
  XCopyArea(display_, pixmap_, target, static_cast<GC>(pixmap_gc_),
            rect.x(), rect.y(), rect.width(), rect.height(),
            rect.x(), rect.y());
}
