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
class SkBitmap;
class TransportDIB;
typedef struct _GdkDrawable GdkDrawable;

// BackingStore ----------------------------------------------------------------

// Represents a backing store for the pixels in a RenderWidgetHost.
class BackingStore {
 public:
#if defined(OS_WIN) || defined(OS_MACOSX)
  BackingStore(RenderWidgetHost* widget, const gfx::Size& size);
#elif defined(OS_LINUX)
  // Create a backing store on the X server. The visual is an Xlib Visual
  // describing the format of the target window and the depth is the color
  // depth of the X window which will be drawn into.
  BackingStore(RenderWidgetHost* widget,
               const gfx::Size& size,
               void* visual,
               int depth);

  // This is for unittesting only. An object constructed using this constructor
  // will silently ignore all paints
  BackingStore(RenderWidgetHost* widget, const gfx::Size& size);
#endif
  ~BackingStore();

  RenderWidgetHost* render_widget_host() const { return render_widget_host_; }
  const gfx::Size& size() { return size_; }

  // The number of bytes that this backing store consumes.  This should roughly
  // be size_.GetArea() * bytes per pixel.
  size_t MemorySize();

#if defined(OS_WIN)
  HDC hdc() { return hdc_; }

  // Returns true if we should convert to the monitor profile when painting.
  static bool ColorManagementEnabled();
#elif defined(OS_MACOSX)
  skia::PlatformCanvas* canvas() { return &canvas_; }
#elif defined(OS_LINUX)
  Display* display() const { return display_; }
  XID root_window() const { return root_window_; };

  // Copy from the server-side backing store to the target window
  //   display: the display of the backing store and target window
  //   damage: the area to copy
  //   target: the X id of the target window
  void ShowRect(const gfx::Rect& damage, XID target);

  // Paints the server-side backing store data to a SkBitmap. On failure, the
  // return bitmap will be isNull().
  SkBitmap PaintRectToBitmap(const gfx::Rect& rect);
#endif

#if defined(TOOLKIT_GTK)
  // Paint the backing store into the target's |dest_rect|.
  void PaintToRect(const gfx::Rect& dest_rect, GdkDrawable* target);
#endif

  // Paints the bitmap from the renderer onto the backing store.
  void PaintRect(base::ProcessHandle process,
                 TransportDIB* bitmap,
                 const gfx::Rect& bitmap_rect);

  // Scrolls the given rect in the backing store, replacing the given region
  // identified by |bitmap_rect| by the bitmap in the file identified by the
  // given file handle.
  void ScrollRect(base::ProcessHandle process,
                  TransportDIB* bitmap, const gfx::Rect& bitmap_rect,
                  int dx, int dy,
                  const gfx::Rect& clip_rect,
                  const gfx::Size& view_size);

 private:
  // The owner of this backing store.
  RenderWidgetHost* render_widget_host_;

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
  void PaintRectWithoutXrender(TransportDIB* bitmap,
                               const gfx::Rect& bitmap_rect);

  // This is the connection to the X server where this backing store will be
  // displayed.
  Display* const display_;
  // If this is true, then |connection_| is good for MIT-SHM (X shared memory).
  const bool use_shared_memory_;
  // If this is true, then we can use Xrender to composite our pixmaps.
  const bool use_render_;
  // If |use_render_| is false, this is the number of bits-per-pixel for |depth|
  int pixmap_bpp_;
  // if |use_render_| is false, we need the Visual to get the RGB masks.
  void* const visual_;
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

  DISALLOW_COPY_AND_ASSIGN(BackingStore);
};

#endif  // CHROME_BROWSER_RENDERER_HOST_BACKING_STORE_H_
