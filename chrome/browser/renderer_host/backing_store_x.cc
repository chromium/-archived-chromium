// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_host/backing_store.h"

#include <utility>

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
                           Drawable parent_window,
                           bool use_shared_memory)
    : size_(size),
      display_(display),
      use_shared_memory_(use_shared_memory),
      parent_window_(parent_window) {
  const int width = size.width();
  const int height = size.height();

  pixmap_ = XCreatePixmap(display_, parent_window, width, height, depth);
  picture_ = XRenderCreatePicture(
      display_, pixmap_,
      x11_util::GetRenderVisualFormat(display_, static_cast<Visual*>(visual)),
      0, NULL);
  pixmap_gc_ = XCreateGC(display_, pixmap_, 0, NULL);
}

BackingStore::BackingStore(const gfx::Size& size)
    : size_(size),
      display_(NULL),
      use_shared_memory_(false),
      parent_window_(0) {
}

BackingStore::~BackingStore() {
  // In unit tests, display_ may be NULL.
  if (!display_)
    return;

  XRenderFreePicture(display_, picture_);
  XFreePixmap(display_, pixmap_);
  XFreeGC(display_, static_cast<GC>(pixmap_gc_));
}

void BackingStore::PaintRect(base::ProcessHandle process,
                             TransportDIB* bitmap,
                             const gfx::Rect& bitmap_rect) {
  if (!display_)
    return;

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
    pixmap = XShmCreatePixmap(display_, parent_window_, NULL, &shminfo, width,
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

    pixmap = XCreatePixmap(display_, parent_window_, width, height, 32);
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
    XCopyArea(display_, pixmap_, pixmap_, static_cast<GC>(pixmap_gc_),
              0, std::max(0, -dy) /* source x, y */,
              size_.width(), size_.height() - abs(dy),
              0, std::max(0, dy) /* dest x, y */);
  } else if (dx) {
    // Positive values of |dx| scroll right
    XCopyArea(display_, pixmap_, pixmap_, static_cast<GC>(pixmap_gc_),
              std::max(0, -dx), 0 /* source x, y */,
              size_.width() - abs(dx), size_.height(),
              std::max(0, dx), 0 /* dest x, y */);
  }

  PaintRect(process, bitmap, bitmap_rect);
}

void BackingStore::ShowRect(const gfx::Rect& rect) {
  XCopyArea(display_, pixmap_, parent_window_, static_cast<GC>(pixmap_gc_),
            rect.x(), rect.y(), rect.width(), rect.height(),
            rect.x(), rect.y());
}
