// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_RENDER_PROCESS_H__
#define CHROME_RENDERER_RENDER_PROCESS_H__

#include "base/timer.h"
#include "chrome/common/child_process.h"
#include "chrome/renderer/render_thread.h"
#include "skia/ext/platform_canvas.h"

namespace gfx {
class Rect;
}

class TransportDIB;

// Represents the renderer end of the browser<->renderer connection. The
// opposite end is the RenderProcessHost. This is a singleton object for
// each renderer.
class RenderProcess : public ChildProcess {
 public:
  static bool GlobalInit(const std::wstring& channel_name);
  static void GlobalCleanup();

  // Returns true if plugins should be loaded in-process.
  static bool ShouldLoadPluginsInProcess();

  // Get a canvas suitable for drawing and transporting to the browser
  //   memory: (output) the transport DIB memory
  //   rect: the rectangle which will be painted, use for sizing the canvas
  //   returns: NULL on error
  //
  // When no longer needed, you should pass the TransportDIB to
  // ReleaseTransportDIB so that it can be recycled.
  static skia::PlatformCanvas* GetDrawingCanvas(
      TransportDIB** memory, const gfx::Rect& rect);

  // Frees shared memory allocated by AllocSharedMemory.  You should only use
  // this function to free the SharedMemory object.
  static void ReleaseTransportDIB(TransportDIB* memory);

 private:
  friend class ChildProcessFactory<RenderProcess>;
  explicit RenderProcess(const std::wstring& channel_name);
  ~RenderProcess();

  // Returns a pointer to the RenderProcess singleton instance.  This is
  // guaranteed to be non-NULL between calls to GlobalInit and GlobalCleanup.
  static RenderProcess* self() {
    return static_cast<RenderProcess*>(child_process_);
  }

  static ChildProcess* ClassFactory(const std::wstring& channel_name);

  // Look in the shared memory cache for a suitable object to reuse.
  //   result: (output) the memory found
  //   size: the resulting memory will be >= this size, in bytes
  //   returns: false if a suitable DIB memory could not be found
  bool GetTransportDIBFromCache(TransportDIB** result, size_t size);

  // Maybe put the given shared memory into the shared memory cache. Returns
  // true if the SharedMemory object was stored in the cache; otherwise, false
  // is returned.
  bool PutSharedMemInCache(TransportDIB* memory);

  void ClearTransportDIBCache();

  // Return the index of a free cache slot in which to install a transport DIB
  // of the given size. If all entries in the cache are larger than the given
  // size, this doesn't free any slots and returns -1.
  int FindFreeCacheSlot(size_t size);

  // Create a new transport DIB of, at least, the given size. Return NULL on
  // error.
  TransportDIB* CreateTransportDIB(size_t size);
  void FreeTransportDIB(TransportDIB*);

  // ChildProcess implementation
  virtual void Cleanup();

  // The one render thread (to be replaced with a set of render threads).
  RenderThread render_thread_;

  // A very simplistic and small cache.  If an entry in this array is non-null,
  // then it points to a SharedMemory object that is available for reuse.
  TransportDIB* shared_mem_cache_[2];

  // This DelayTimer cleans up our cache 5 seconds after the last use.
  base::DelayTimer<RenderProcess> shared_mem_cache_cleaner_;

  // TransportDIB sequence number
  uint32 sequence_number_;

  static bool load_plugins_in_process_;

  DISALLOW_COPY_AND_ASSIGN(RenderProcess);
};

#endif  // CHROME_RENDERER_RENDER_PROCESS_H__
