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
                                           const gfx::Rect &bitmap_rect) {
  const int width = bitmap_rect.width();
  const int height = bitmap_rect.height();
  Pixmap pixmap = XCreatePixmap(display_, root_window_, width, height,
                                visual_depth_);

  XImage image;
  memset(&image, 0, sizeof(image));

  image.width = width;
  image.height = height;
  image.format = ZPixmap;
  image.byte_order = LSBFirst;
  image.bitmap_unit = 8;
  image.bitmap_bit_order = LSBFirst;
  image.red_mask = 0xff;
  image.green_mask = 0xff00;
  image.blue_mask = 0xff0000;

  if (pixmap_bpp_ == 32) {
    // If the X server depth is already 32-bits, then our job is easy.
    image.depth = visual_depth_;
    image.bits_per_pixel = 32;
    image.bytes_per_line = width * 4;
    image.data = static_cast<char*>(bitmap->memory());

    XPutImage(display_, pixmap, static_cast<GC>(pixmap_gc_), &image,
              0, 0 /* source x, y */, 0, 0 /* dest x, y */,
              width, height);
  } else if (pixmap_bpp_ == 24) {
    // In this case we just need to strip the alpha channel out of each
    // pixel. This is the case which covers VNC servers since they don't
    // support Xrender but typically have 24-bit visuals.
    //
    // It's possible to use some fancy SSE tricks here, but since this is the
    // slow path anyway, we do it slowly.

    uint8_t* bitmap24 = static_cast<uint8_t*>(malloc(3 * width * height));
    const uint32_t* bitmap_in = static_cast<const uint32_t*>(bitmap->memory());
    for (int y = 0; y < height; ++y) {
      for (int x = 0; x < width; ++x) {
        const uint32_t pixel = *(bitmap_in++);
        bitmap24[0] = (pixel >> 16) & 0xff;
        bitmap24[1] = (pixel >> 8) & 0xff;
        bitmap24[2] = pixel & 0xff;
        bitmap24 += 3;
      }
    }

    image.depth = visual_depth_;
    image.bits_per_pixel = 24;
    image.bytes_per_line = width * 3;
    image.data = reinterpret_cast<char*>(bitmap24);

    XPutImage(display_, pixmap, static_cast<GC>(pixmap_gc_), &image,
              0, 0 /* source x, y */, 0, 0 /* dest x, y */,
              width, height);

    free(bitmap24);
  } else {
    CHECK(false) << "Sorry, we don't support your visual depth without "
                    "Xrender support (depth:" << visual_depth_
                 << " bpp:" << pixmap_bpp_ << ")";
  }

  XCopyArea(display_, pixmap /* source */, pixmap_ /* target */,
            static_cast<GC>(pixmap_gc_),
            0, 0 /* source x, y */, bitmap_rect.width(), bitmap_rect.height(),
            bitmap_rect.x(), bitmap_rect.y() /* dest x, y */);

  XFreePixmap(display_, pixmap);
}

void BackingStore::PaintRect(base::ProcessHandle process,
                             TransportDIB* bitmap,
                             const gfx::Rect& bitmap_rect) {
  if (!display_)
    return;

  if (!use_render_)
    return PaintRectWithoutXrender(bitmap, bitmap_rect);

  const int width = bitmap_rect.width();
  const int height = bitmap_rect.height();
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
    pixmap = XShmCreatePixmap(display_, root_window_, NULL, &shminfo, width,
                              height, 32);
  } else {
    // No shared memory support, we have to copy the bitmap contents to the X
    // server. Xlib wraps the underlying PutImage call behind several layers of
    // functions which try to convert the image into the format which the X
    // server expects. The following values hopefully disable all conversions.
    XImage image;
    memset(&image, 0, sizeof(image));

    image.width = width;
    image.height = height;
    image.depth = 32;
    image.bits_per_pixel = 32;
    image.format = ZPixmap;
    image.byte_order = LSBFirst;
    image.bitmap_unit = 8;
    image.bitmap_bit_order = LSBFirst;
    image.bytes_per_line = width * 4;
    image.red_mask = 0xff;
    image.green_mask = 0xff00;
    image.blue_mask = 0xff0000;
    image.data = static_cast<char*>(bitmap->memory());

    pixmap = XCreatePixmap(display_, root_window_, width, height, 32);
    GC gc = XCreateGC(display_, pixmap, 0, NULL);
    XPutImage(display_, pixmap, gc, &image,
              0, 0 /* source x, y */, 0, 0 /* dest x, y */,
              width, height);
    XFreeGC(display_, gc);
  }

  picture = x11_util::CreatePictureFromSkiaPixmap(display_, pixmap);
  XRenderComposite(display_, PictOpSrc, picture /* source */, 0 /* mask */,
                   picture_ /* dest */, 0, 0 /* source x, y */,
                   0, 0 /* mask x, y */,
                   bitmap_rect.x(), bitmap_rect.y() /* target x, y */,
                   width, height);

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
    if (abs(dy) >= clip_rect.height())
      return;

    XCopyArea(display_, pixmap_, pixmap_, static_cast<GC>(pixmap_gc_),
              clip_rect.x() /* source x */,
              std::max(clip_rect.y(), clip_rect.y() - dy),
              clip_rect.width(),
              clip_rect.height() - abs(dy),
              clip_rect.x() /* dest x */,
              std::max(clip_rect.y(), clip_rect.y() + dy) /* dest y */);
  } else if (dx) {
    // Positive values of |dx| scroll right
    if (abs(dx) >= clip_rect.width())
      return;

    XCopyArea(display_, pixmap_, pixmap_, static_cast<GC>(pixmap_gc_),
              std::max(clip_rect.x(), clip_rect.x() - dx),
              clip_rect.y() /* source y */,
              clip_rect.width() - abs(dx),
              clip_rect.height(),
              std::max(clip_rect.x(), clip_rect.x() + dx) /* dest x */,
              clip_rect.y() /* dest x */);
  }

  PaintRect(process, bitmap, bitmap_rect);
}

void BackingStore::ShowRect(const gfx::Rect& rect, XID target) {
  XCopyArea(display_, pixmap_, target, static_cast<GC>(pixmap_gc_),
            rect.x(), rect.y(), rect.width(), rect.height(),
            rect.x(), rect.y());
}
