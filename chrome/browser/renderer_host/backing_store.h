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
  //   parent_window: The X id of the target window
  //   use_shared_memory: if true, the X server is local
  BackingStore(const gfx::Size& size, Display* x_connection, int depth,
               void* visual, XID parent_window, bool use_shared_memory);
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
  void ShowRect(const gfx::Rect& damage);
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
  // The size of the backing store.
  gfx::Size size_;

#if defined(OS_WIN)
  // Creates a dib conforming to the height/width/section parameters passed
  // in. The use_os_color_depth parameter controls whether we use the color
  // depth to create an appropriate dib or not.
  HANDLE CreateDIB(HDC dc,
                   int width, int height,
                   bool use_os_color_depth,
                   HANDLE section);

  // The backing store dc.
  HDC hdc_;

  // Handle to the backing store dib.
  HANDLE backing_store_dib_;

  // Handle to the original bitmap in the dc.
  HANDLE original_bitmap_;
#elif defined(OS_MACOSX)
  skia::PlatformCanvas canvas_;
#elif defined(OS_LINUX)
  // This is the connection to the X server where this backing store will be
  // displayed.
  Display *const display_;
  // If this is true, then |connection_| is good for MIT-SHM (X shared memory).
  const bool use_shared_memory_;
  // The parent window (probably a GtkDrawingArea) for this backing store.
  const XID parent_window_;
  // This is a handle to the server side pixmap which is our backing store.
  XID pixmap_;
  // This is the RENDER picture pointing at |pixmap_|.
  XID picture_;
  // This is a default graphic context, used in XCopyArea
  void* pixmap_gc_;
#endif

  DISALLOW_COPY_AND_ASSIGN(BackingStore);
};

// BackingStoreManager ---------------------------------------------------------

// This class manages backing stores in the browsr. Every RenderWidgetHost is
// associated with a backing store which it requests from this class.  The
// hosts don't maintain any references to the backing stores.  These backing
// stores are maintained in a cache which can be trimmed as needed.
class BackingStoreManager {
 public:
  // Returns a backing store which matches the desired dimensions.
  //
  // backing_store_rect
  //   The desired backing store dimensions.
  // Returns a pointer to the backing store on success, NULL on failure.
  static BackingStore* GetBackingStore(RenderWidgetHost* host,
                                       const gfx::Size& desired_size);

  // Returns a backing store which is fully ready for consumption, i.e. the
  // bitmap from the renderer has been copied into the backing store dc, or the
  // bitmap in the backing store dc references the renderer bitmap.
  //
  // backing_store_size
  //   The desired backing store dimensions.
  // process_handle
  //   The renderer process handle.
  // bitmap_section
  //   The bitmap section from the renderer.
  // bitmap_rect
  //   The rect to be painted into the backing store
  // needs_full_paint
  //   Set if we need to send out a request to paint the view
  //   to the renderer.
  static BackingStore* PrepareBackingStore(RenderWidgetHost* host,
                                           const gfx::Size& backing_store_size,
                                           base::ProcessHandle process_handle,
                                           TransportDIB* bitmap,
                                           const gfx::Rect& bitmap_rect,
                                           bool* needs_full_paint);

  // Returns a matching backing store for the host.
  // Returns NULL if we fail to find one.
  static BackingStore* Lookup(RenderWidgetHost* host);

  // Removes the backing store for the host.
  static void RemoveBackingStore(RenderWidgetHost* host);

 private:
  // Not intended for instantiation.
  BackingStoreManager() {}

  DISALLOW_COPY_AND_ASSIGN(BackingStoreManager);
};

#endif  // CHROME_BROWSER_RENDERER_HOST_BACKING_STORE_H_
