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
#include "base/file_util.h"
#include "base/message_loop.h"
#include "base/histogram.h"
#include "base/path_service.h"
#include "base/sys_info.h"
// TODO(jar): DNS calls should be renderer specific, not including browser.
#include "chrome/browser/net/dns_global.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/ipc_channel.h"
#include "chrome/common/ipc_message_utils.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/transport_dib.h"
#include "chrome/renderer/render_view.h"
#include "media/base/media.h"
#include "webkit/glue/webkit_glue.h"

static size_t GetMaxSharedMemorySize() {
  static int size = 0;
#if defined(OS_LINUX)
  if (size == 0) {
    std::string contents;
    file_util::ReadFileToString(FilePath("/proc/sys/kernel/shmmax"), &contents);
    size = strtoul(contents.c_str(), NULL, 0);
  }
#endif
  return size;
}

//-----------------------------------------------------------------------------

RenderProcess::RenderProcess()
    : ChildProcess(new RenderThread()),
      ALLOW_THIS_IN_INITIALIZER_LIST(shared_mem_cache_cleaner_(
          base::TimeDelta::FromSeconds(5),
          this, &RenderProcess::ClearTransportDIBCache)),
      sequence_number_(0) {
  Init();
}

RenderProcess::RenderProcess(const std::string& channel_name)
    : ChildProcess(new RenderThread(channel_name)),
      ALLOW_THIS_IN_INITIALIZER_LIST(shared_mem_cache_cleaner_(
          base::TimeDelta::FromSeconds(5),
          this, &RenderProcess::ClearTransportDIBCache)),
      sequence_number_(0) {
  Init();
}

RenderProcess::~RenderProcess() {
  // TODO(port)
  // Try and limit what we pull in for our non-Win unit test bundle
#ifndef NDEBUG
  // log important leaked objects
  webkit_glue::CheckForLeaks();
#endif

  GetShutDownEvent()->Signal();

  // We need to stop the RenderThread as the clearer_factory_
  // member could be in use while the object itself is destroyed,
  // as a result of the containing RenderProcess object being destroyed.
  // This race condition causes a crash when the renderer process is shutting
  // down.
  child_thread()->Stop();
  ClearTransportDIBCache();
}

void RenderProcess::Init() {
  in_process_plugins_ = InProcessPlugins();
  for (size_t i = 0; i < arraysize(shared_mem_cache_); ++i)
    shared_mem_cache_[i] = NULL;

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

  // Out of process dev tools rely upon auto break behavior.
  webkit_glue::SetJavaScriptFlags(
      L"--debugger-auto-break"
      // Enable lazy in-memory profiling.
      L" --prof --prof-lazy --logfile=* --compress-log");

  if (command_line.HasSwitch(switches::kEnableWatchdog)) {
    // TODO(JAR): Need to implement renderer IO msgloop watchdog.
  }

  if (command_line.HasSwitch(switches::kDumpHistogramsOnExit)) {
    StatisticsRecorder::set_dump_on_exit(true);
  }

  FilePath module_path;
  initialized_media_library_ =
      PathService::Get(base::DIR_MODULE, &module_path) &&
      media::InitializeMediaLibrary(module_path);
}

bool RenderProcess::InProcessPlugins() {
  const CommandLine& command_line = *CommandLine::ForCurrentProcess();
#if defined(OS_LINUX)
  // Plugin processes require a UI message loop, and the Linux message loop
  // implementation only allows one UI loop per process.
  if (command_line.HasSwitch(switches::kInProcessPlugins))
    NOTIMPLEMENTED() << ": in process plugins not supported on Linux";
  return command_line.HasSwitch(switches::kInProcessPlugins);
#else
  return command_line.HasSwitch(switches::kInProcessPlugins) ||
         command_line.HasSwitch(switches::kSingleProcess);
#endif
}

// -----------------------------------------------------------------------------
// Platform specific code for dealing with bitmap transport...

TransportDIB* RenderProcess::CreateTransportDIB(size_t size) {
#if defined(OS_WIN) || defined(OS_LINUX)
  // Windows and Linux create transport DIBs inside the renderer
  return TransportDIB::Create(size, sequence_number_++);
#elif defined(OS_MACOSX)  // defined(OS_WIN) || defined(OS_LINUX)
  // Mac creates transport DIBs in the browser, so we need to do a sync IPC to
  // get one.
  TransportDIB::Handle handle;
  IPC::Message* msg = new ViewHostMsg_AllocTransportDIB(size, &handle);
  if (!child_thread()->Send(msg))
    return NULL;
  if (handle.fd < 0)
    return NULL;
  return TransportDIB::Map(handle);
#endif  // defined(OS_MACOSX)
}

void RenderProcess::FreeTransportDIB(TransportDIB* dib) {
  if (!dib)
    return;

#if defined(OS_MACOSX)
  // On Mac we need to tell the browser that it can drop a reference to the
  // shared memory.
  IPC::Message* msg = new ViewHostMsg_FreeTransportDIB(dib->id());
  child_thread()->Send(msg);
#endif

  delete dib;
}

// -----------------------------------------------------------------------------


skia::PlatformCanvas* RenderProcess::GetDrawingCanvas(
    TransportDIB** memory, const gfx::Rect& rect) {
  int width = rect.width();
  int height = rect.height();
  const size_t stride = skia::PlatformCanvas::StrideForWidth(rect.width());
  const size_t max_size = GetMaxSharedMemorySize();

  // If the requested size is too big, reduce the height. Ideally we might like
  // to reduce the width as well to make the size reduction more "balanced", but
  // it rarely comes up in practice.
  if ((max_size != 0) && (height * stride > max_size))
    height = max_size / stride;

  const size_t size = height * stride;

  if (!GetTransportDIBFromCache(memory, size)) {
    *memory = CreateTransportDIB(size);
    if (!*memory)
      return false;
  }

  return (*memory)->GetPlatformCanvas(width, height);
}

void RenderProcess::ReleaseTransportDIB(TransportDIB* mem) {
  if (PutSharedMemInCache(mem)) {
    shared_mem_cache_cleaner_.Reset();
    return;
  }

  FreeTransportDIB(mem);
}

bool RenderProcess::GetTransportDIBFromCache(TransportDIB** mem,
                                             size_t size) {
  // look for a cached object that is suitable for the requested size.
  for (size_t i = 0; i < arraysize(shared_mem_cache_); ++i) {
    if (shared_mem_cache_[i] &&
        size <= shared_mem_cache_[i]->size()) {
      *mem = shared_mem_cache_[i];
      shared_mem_cache_[i] = NULL;
      return true;
    }
  }

  return false;
}

int RenderProcess::FindFreeCacheSlot(size_t size) {
  // simple algorithm:
  //  - look for an empty slot to store mem, or
  //  - if full, then replace smallest entry which is smaller than |size|
  for (size_t i = 0; i < arraysize(shared_mem_cache_); ++i) {
    if (shared_mem_cache_[i] == NULL)
      return i;
  }

  size_t smallest_size = size;
  int smallest_index = -1;

  for (size_t i = 1; i < arraysize(shared_mem_cache_); ++i) {
    const size_t entry_size = shared_mem_cache_[i]->size();
    if (entry_size < smallest_size) {
      smallest_size = entry_size;
      smallest_index = i;
    }
  }

  if (smallest_index != -1) {
    FreeTransportDIB(shared_mem_cache_[smallest_index]);
    shared_mem_cache_[smallest_index] = NULL;
  }

  return smallest_index;
}

bool RenderProcess::PutSharedMemInCache(TransportDIB* mem) {
  const int slot = FindFreeCacheSlot(mem->size());
  if (slot == -1)
    return false;

  shared_mem_cache_[slot] = mem;
  return true;
}

void RenderProcess::ClearTransportDIBCache() {
  for (size_t i = 0; i < arraysize(shared_mem_cache_); ++i) {
    if (shared_mem_cache_[i]) {
      FreeTransportDIB(shared_mem_cache_[i]);
      shared_mem_cache_[i] = NULL;
    }
  }
}
