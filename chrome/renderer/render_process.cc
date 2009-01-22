// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"

#if defined(OS_WIN)
#include <windows.h>
#include <objidl.h>
#include <mlang.h>
#endif

#include "chrome/renderer/render_process.h"

#include "base/basictypes.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/message_loop.h"
#include "base/histogram.h"
#include "base/path_service.h"
#include "base/sys_info.h"
#include "chrome/browser/net/dns_global.h"  // TODO(jar): DNS calls should be renderer specific, not including browser.
#include "chrome/common/chrome_switches.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/ipc_channel.h"
#include "chrome/common/ipc_message_utils.h"
#include "chrome/common/render_messages.h"
#include "chrome/renderer/render_view.h"
#include "webkit/glue/webkit_glue.h"

//-----------------------------------------------------------------------------

bool RenderProcess::load_plugins_in_process_ = false;

//-----------------------------------------------------------------------------

RenderProcess::RenderProcess(const std::wstring& channel_name)
    : render_thread_(channel_name),
      ALLOW_THIS_IN_INITIALIZER_LIST(clearer_factory_(this)) {
  for (size_t i = 0; i < arraysize(shared_mem_cache_); ++i)
    shared_mem_cache_[i] = NULL;
}

RenderProcess::~RenderProcess() {
  // We need to stop the RenderThread as the clearer_factory_
  // member could be in use while the object itself is destroyed,
  // as a result of the containing RenderProcess object being destroyed.
  // This race condition causes a crash when the renderer process is shutting
  // down.
  render_thread_.Stop();
  ClearSharedMemCache();
}

// static
bool RenderProcess::GlobalInit(const std::wstring &channel_name) {
#if defined(OS_WIN)
  // HACK:  See http://b/issue?id=1024307 for rationale.
  if (GetModuleHandle(L"LPK.DLL") == NULL) {
    // Makes sure lpk.dll is loaded by gdi32 to make sure ExtTextOut() works
    // when buffering into a EMF buffer for printing.
    typedef BOOL (__stdcall *GdiInitializeLanguagePack)(int LoadedShapingDLLs);
    GdiInitializeLanguagePack gdi_init_lpk =
        reinterpret_cast<GdiInitializeLanguagePack>(GetProcAddress(
            GetModuleHandle(L"GDI32.DLL"),
            "GdiInitializeLanguagePack"));
    DCHECK(gdi_init_lpk);
    if (gdi_init_lpk) {
      gdi_init_lpk(0);
    }
  }
#endif

  const CommandLine& command_line = *CommandLine::ForCurrentProcess();
  if (command_line.HasSwitch(switches::kJavaScriptFlags)) {
    webkit_glue::SetJavaScriptFlags(
      command_line.GetSwitchValue(switches::kJavaScriptFlags));
  }
  if (command_line.HasSwitch(switches::kPlaybackMode) ||
      command_line.HasSwitch(switches::kRecordMode)) {
      webkit_glue::SetRecordPlaybackMode(true);
  }

  if (command_line.HasSwitch(switches::kInProcessPlugins) ||
      command_line.HasSwitch(switches::kSingleProcess))
    load_plugins_in_process_ = true;

  if (command_line.HasSwitch(switches::kEnableWatchdog)) {
    // TODO(JAR): Need to implement renderer IO msgloop watchdog.
  }

  if (command_line.HasSwitch(switches::kDumpHistogramsOnExit)) {
    StatisticsRecorder::set_dump_on_exit(true);
  }

  if (command_line.HasSwitch(switches::kGearsInRenderer)) {
#if defined(OS_WIN)
    // Load gears.dll on startup so we can access it before the sandbox
    // blocks us.
    std::wstring path;
    if (PathService::Get(chrome::FILE_GEARS_PLUGIN, &path))
      LoadLibrary(path.c_str());
#else
    // TODO(port) Need to handle loading gears on non-Windows platforms
    NOTIMPLEMENTED();
#endif
  }

  ChildProcessFactory<RenderProcess> factory;
  return ChildProcess::GlobalInit(channel_name, &factory);
}

// static
void RenderProcess::GlobalCleanup() {
  ChildProcess::GlobalCleanup();
}

// static
bool RenderProcess::ShouldLoadPluginsInProcess() {
  return load_plugins_in_process_;
}

// static
base::SharedMemory* RenderProcess::AllocSharedMemory(size_t size) {
  self()->clearer_factory_.RevokeAll();

  base::SharedMemory* mem = self()->GetSharedMemFromCache(size);
  if (mem)
    return mem;

  // Round-up size to allocation granularity
  size_t allocation_granularity = base::SysInfo::VMAllocationGranularity();
  size = size / allocation_granularity + 1;
  size = size * allocation_granularity;

  mem = new base::SharedMemory();
  if (!mem)
    return NULL;
  if (!mem->Create(L"", false, true, size)) {
    delete mem;
    return NULL;
  }

  return mem;
}

// static
void RenderProcess::FreeSharedMemory(base::SharedMemory* mem) {
  if (self()->PutSharedMemInCache(mem)) {
    self()->ScheduleCacheClearer();
    return;
  }
  DeleteSharedMem(mem);
}

// static
void RenderProcess::DeleteSharedMem(base::SharedMemory* mem) {
  delete mem;
}

base::SharedMemory* RenderProcess::GetSharedMemFromCache(size_t size) {
  // look for a cached object that is suitable for the requested size.
  for (size_t i = 0; i < arraysize(shared_mem_cache_); ++i) {
    base::SharedMemory* mem = shared_mem_cache_[i];
    if (mem && mem->max_size() >= size) {
      shared_mem_cache_[i] = NULL;
      return mem;
    }
  }
  return NULL;
}

bool RenderProcess::PutSharedMemInCache(base::SharedMemory* mem) {
  // simple algorithm:
  //  - look for an empty slot to store mem, or
  //  - if full, then replace any existing cache entry that is smaller than the
  //    given shared memory object.
  for (size_t i = 0; i < arraysize(shared_mem_cache_); ++i) {
    if (!shared_mem_cache_[i]) {
      shared_mem_cache_[i] = mem;
      return true;
    }
  }
  for (size_t i = 0; i < arraysize(shared_mem_cache_); ++i) {
    base::SharedMemory* cached_mem = shared_mem_cache_[i];
    if (cached_mem->max_size() < mem->max_size()) {
      shared_mem_cache_[i] = mem;
      DeleteSharedMem(cached_mem);
      return true;
    }
  }
  return false;
}

void RenderProcess::ClearSharedMemCache() {
  for (size_t i = 0; i < arraysize(shared_mem_cache_); ++i) {
    if (shared_mem_cache_[i]) {
      DeleteSharedMem(shared_mem_cache_[i]);
      shared_mem_cache_[i] = NULL;
    }
  }
}

void RenderProcess::ScheduleCacheClearer() {
  // If we already have a deferred clearer, then revoke it so we effectively
  // delay cache clearing until idle for our desired interval.
  clearer_factory_.RevokeAll();

  MessageLoop::current()->PostDelayedTask(FROM_HERE,
      clearer_factory_.NewRunnableMethod(&RenderProcess::ClearSharedMemCache),
      5000 /* 5 seconds */);
}

void RenderProcess::Cleanup() {
#ifndef NDEBUG
  // log important leaked objects
  webkit_glue::CheckForLeaks();
#endif
}
