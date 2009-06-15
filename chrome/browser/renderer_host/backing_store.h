// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RENDERER_HOST_BACKING_STORE_H_
#define CHROME_BROWSER_RENDERER_HOST_BACKING_STORE_H_

#include "base/basictypes.h"
#include "base/gfx/rect.h"
#include "base/gfx/size.h"
#include "base/process.h"
#include "build/build_config.h"
#include "chrome/common/mru_cache.h"

#if defined(OS_WIN)
#include <windows.h>
#elif defined(OS_MACOSX)
#include "skia/ext/platform_canvas.h"
#elif defined(OS_LINUX)
#include "chrome/common/x11_util.h"
#endif

class RenderWidgetHost;
class TransportDIB;

// BackingStore ----------------------------------------------------------------

// Represents a backing store for the pixels in a RenderWidgetHost.
class BackingStore {
 public:
#if defined(OS_WIN) || defined(OS_MACOSX)
  explicit BackingStore(const gfx::Size& size);
#elif defined(OS_LINUX)
  // Create a backing store on the X server.
  //   size: the size of the server-side pixmap
  //   x_connection: the display to target
  //   depth: the depth of the X window which will be drawn into
  //   visual: An Xlib Visual describing the format of the target window
  //   root_window: The X id of the root window
  //   use_render: if true, the X server supports Xrender
  //   use_shared_memory: if true, the X server is local
  BackingStore(const gfx::Size& size, Display* x_connection, int depth,
               void* visual, XID root_window, bool use_render,
               bool use_shared_memory);
  // This is for unittesting only. An object constructed using this constructor
  // will silently ignore all paints
  explicit BackingStore(const gfx::Size& size);
#endif
  ~BackingStore();

  const gfx::Size& size() { return size_; }

#if defined(OS_WIN)
  HDC hdc() { return hdc_; }
#elif defined(OS_MACOSX)
  skia::PlatformCanvas* canvas() { return &canvas_; }
#elif defined(OS_LINUX)
  // Copy from the server-side backing store to the target window
  //   display: the display of the backing store and target window
  //   damage: the area to copy
  //   target: the X id of the target window
  void ShowRect(const gfx::Rect& damage, XID target);
#endif

  // Paints the bitmap from the renderer onto the backing store.
  // bitmap_rect is the rect of the whole bitmap, and paint_rect
  // is a sub-rect of bitmap that we want to draw.
  void PaintRect(base::ProcessHandle process,
                 TransportDIB* bitmap,
                 const gfx::Rect& bitmap_rect,
                 const gfx::Rect& paint_rect);

  // Scrolls the given rect in the backing store, replacing the given region
  // identified by |bitmap_rect| by the bitmap in the file identified by the
  // given file handle.
  void ScrollRect(base::ProcessHandle process,
                  TransportDIB* bitmap, const gfx::Rect& bitmap_rect,
                  int dx, int dy,
                  const gfx::Rect& clip_rect,
                  const gfx::Size& view_size);

 private:
  // The size of the backing store.
  gfx::Size size_;

#if defined(OS_WIN)

  // The backing store dc.
  HDC hdc_;
  // Handle to the backing store dib.
  HANDLE backing_store_dib_;
  // Handle to the original bitmap in the dc.
  HANDLE original_bitmap_;
  // Number of bits per pixel of the screen.
  int color_depth_;
#elif defined(OS_MACOSX)
  skia::PlatformCanvas canvas_;
#elif defined(OS_LINUX)
  // Paints the bitmap from the renderer onto the backing store without
  // using Xrender to composite the pixmaps.
  // bitmap_rect is the rect of the whole bitmap, and paint_rect
  // is a sub-rect of bitmap that we want to draw.
  void PaintRectWithoutXrender(TransportDIB* bitmap,
                               const gfx::Rect& bitmap_rect,
                               const gfx::Rect& paint_rect);

  // This is the connection to the X server where this backing store will be
  // displayed.
  Display *const display_;
  // If this is true, then |connection_| is good for MIT-SHM (X shared memory).
  const bool use_shared_memory_;
  // If this is true, then we can use Xrender to composite our pixmaps.
  const bool use_render_;
  // If |use_render_| is false, this is the number of bits-per-pixel for |depth|
  int pixmap_bpp_;
  // This is the depth of the target window.
  const int visual_depth_;
  // The parent window (probably a GtkDrawingArea) for this backing store.
  const XID root_window_;
  // This is a handle to the server side pixmap which is our backing store.
  XID pixmap_;
  // This is the RENDER picture pointing at |pixmap_|.
  XID picture_;
  // This is a default graphic context, used in XCopyArea
  void* pixmap_gc_;
#endif

  // Sanity checks on the size of the rects to draw so that we don't allocate
  // enourmous pixmaps.
  static int kMaxBitmapLengthAllowed;

  DISALLOW_COPY_AND_ASSIGN(BackingStore);
};

#endif  // CHROME_BROWSER_RENDERER_HOST_BACKING_STORE_H_
