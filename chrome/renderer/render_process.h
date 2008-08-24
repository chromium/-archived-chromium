// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_RENDER_PROCESS_H__
#define CHROME_RENDERER_RENDER_PROCESS_H__

#include <objidl.h>
#include <mlang.h>

#include "base/shared_memory.h"
#include "chrome/common/child_process.h"
#include "chrome/renderer/render_thread.h"

class RenderView;

// Represents the renderer end of the browser<->renderer connection. The
// opposite end is the RenderProcessHost. This is a singleton object for
// each renderer.
class RenderProcess : public ChildProcess {
 public:
  static bool GlobalInit(const std::wstring& channel_name);
  static void GlobalCleanup();

  // Returns true if plugins should be loaded in-process.
  static bool ShouldLoadPluginsInProcess();

  static IMLangFontLink2* GetLangFontLink();

  // Allocates shared memory.  When no longer needed, you should pass the
  // SharedMemory pointer to FreeSharedMemory so it can be recycled.  The size
  // reported in the resulting SharedMemory object will be greater than or
  // equal to the requested size.  This method returns NULL if unable to
  // allocate memory for some reason.
  static SharedMemory* AllocSharedMemory(size_t size);

  // Frees shared memory allocated by AllocSharedMemory.  You should only use
  // this function to free the SharedMemory object.
  static void FreeSharedMemory(SharedMemory* mem);

  // Deletes the shared memory allocated by AllocSharedMemory.
  static void DeleteSharedMem(SharedMemory* mem);

 private:
  friend class ChildProcessFactory<RenderProcess>;
  RenderProcess(const std::wstring& channel_name);
  ~RenderProcess();

  // Initializes the LangFontLink object. This function cannot be called
  // while the process is restricted by the sandbox or it will fail.
  static void InitializeLangFontLink();

  // Releases the LangFontLink object if already created.
  static void ReleaseLangFontLink();

  // Returns a pointer to the RenderProcess singleton instance.  This is
  // guaranteed to be non-NULL between calls to GlobalInit and GlobalCleanup.
  static RenderProcess* self() {
    return static_cast<RenderProcess*>(child_process_);
  }

  static ChildProcess* ClassFactory(const std::wstring& channel_name);

  // Look in the shared memory cache for a suitable object to reuse.  Returns
  // NULL if there is none.
  SharedMemory* GetSharedMemFromCache(size_t size);

  // Maybe put the given shared memory into the shared memory cache.  Returns
  // true if the SharedMemory object was stored in the cache; otherwise, false
  // is returned.
  bool PutSharedMemInCache(SharedMemory* mem);

  void ClearSharedMemCache();

  // We want to lazily clear the shared memory cache if no one has requested
  // memory.  This methods are used to schedule a deferred call to
  // RenderProcess::ClearSharedMemCache.
  void ScheduleCacheClearer();

  // ChildProcess implementation
  virtual void Cleanup();

  // The one render thread (to be replaced with a set of render threads).
  RenderThread render_thread_;

  // A very simplistic and small cache.  If an entry in this array is non-null,
  // then it points to a SharedMemory object that is available for reuse.
  SharedMemory* shared_mem_cache_[2];

  // This factory is used to lazily invoke ClearSharedMemCache.
  ScopedRunnableMethodFactory<RenderProcess> clearer_factory_;

  static IMLangFontLink2* lang_font_link_;

  static bool load_plugins_in_process_;

  DISALLOW_EVIL_CONSTRUCTORS(RenderProcess);
};

#endif  // CHROME_RENDERER_RENDER_PROCESS_H__

