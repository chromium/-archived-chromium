// Copyright 2008, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include <windows.h>
#include <objidl.h>
#include <mlang.h>

#include "chrome/renderer/render_process.h"

#include "base/basictypes.h"
#include "base/command_line.h"
#include "base/message_loop.h"
#include "base/histogram.h"
#include "chrome/browser/net/dns_global.h"  // TODO(jar): DNS calls should be renderer specific, not including browser.
#include "chrome/common/chrome_switches.h"
#include "chrome/common/ipc_channel.h"
#include "chrome/common/ipc_message_utils.h"
#include "chrome/common/render_messages.h"
#include "chrome/renderer/render_view.h"
#include "webkit/glue/webkit_glue.h"

//-----------------------------------------------------------------------------

IMLangFontLink2* RenderProcess::lang_font_link_ = NULL;
bool RenderProcess::load_plugins_in_process_ = false;

//-----------------------------------------------------------------------------

RenderProcess::RenderProcess(const std::wstring& channel_name)
    : render_thread_(channel_name),
#pragma warning(suppress: 4355)  // Okay to pass "this" here.
      clearer_factory_(this) {
  for (int i = 0; i < arraysize(shared_mem_cache_); ++i)
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

  InitializeLangFontLink();

  CommandLine command_line;
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

  if (command_line.HasSwitch(switches::kDnsPrefetchDisable)) {
    chrome_browser_net::EnableDnsPrefetch(false);
  }

  if (command_line.HasSwitch(switches::kEnableWatchdog)) {
    // TODO(JAR): Need to implement renderer IO msgloop watchdog.
  }

  if (command_line.HasSwitch(switches::kDumpHistogramsOnExit)) {
    StatisticsRecorder::set_dump_on_exit(true);
  }

  ChildProcessFactory<RenderProcess> factory;
  return ChildProcess::GlobalInit(channel_name, &factory);
}

// static
void RenderProcess::GlobalCleanup() {
  ChildProcess::GlobalCleanup();
  ReleaseLangFontLink();
}

// static
void RenderProcess::InitializeLangFontLink() {
  // TODO(hbono): http://b/1072298 Experimentally commented out this code to
  // prevent registry leaks caused by this IMLangFontLink2 interface.
  // If you find any font-rendering regressions. Please feel free to blame me.
#ifdef USE_IMLANGFONTLINK2
  IMultiLanguage* multi_language = NULL;
  lang_font_link_ = NULL;
  if (S_OK != CoCreateInstance(CLSID_CMultiLanguage,
                               0,
                               CLSCTX_ALL,
                               IID_IMultiLanguage,
                               reinterpret_cast<void**>(&multi_language))) {
    DLOG(ERROR) << "Cannot CoCreate CMultiLanguage";
  } else {
    if (S_OK != multi_language->QueryInterface(IID_IMLangFontLink2,
        reinterpret_cast<void**>(&lang_font_link_))) {
      DLOG(ERROR) << "Cannot query LangFontLink2 interface";
    }
  }

  if (multi_language)
    multi_language->Release();
#endif
}

// static
void RenderProcess::ReleaseLangFontLink() {
  // TODO(hbono): http://b/1072298 Experimentally commented out this code to
  // prevent registry leaks caused by this IMLangFontLink2 interface.
  // If you find any font-rendering regressions. Please feel free to blame me.
#ifdef USE_IMLANGFONTLINK2
  if (lang_font_link_)
    lang_font_link_->Release();
#endif
}

// static
IMLangFontLink2* RenderProcess::GetLangFontLink() {
  return lang_font_link_;
}

// static
bool RenderProcess::ShouldLoadPluginsInProcess() {
  return load_plugins_in_process_;
}

// static
SharedMemory* RenderProcess::AllocSharedMemory(size_t size) {
  self()->clearer_factory_.RevokeAll();

  SharedMemory* mem = self()->GetSharedMemFromCache(size);
  if (mem)
    return mem;

  // Round-up size to allocation granularity
  SYSTEM_INFO info;
  GetSystemInfo(&info);

  size = size / info.dwAllocationGranularity + 1;
  size = size * info.dwAllocationGranularity;

  mem = new SharedMemory();
  if (!mem)
    return NULL;
  if (!mem->Create(L"", false, true, size)) {
    delete mem;
    return NULL;
  }

  return mem;
}

// static
void RenderProcess::FreeSharedMemory(SharedMemory* mem) {
  if (self()->PutSharedMemInCache(mem)) {
    self()->ScheduleCacheClearer();
    return;
  }
  DeleteSharedMem(mem);
}

// static
void RenderProcess::DeleteSharedMem(SharedMemory* mem) {
  delete mem;
}

SharedMemory* RenderProcess::GetSharedMemFromCache(size_t size) {
  // look for a cached object that is suitable for the requested size.
  for (int i = 0; i < arraysize(shared_mem_cache_); ++i) {
    SharedMemory* mem = shared_mem_cache_[i];
    if (mem && mem->max_size() >= size) {
      shared_mem_cache_[i] = NULL;
      return mem;
    }
  }
  return NULL;
}

bool RenderProcess::PutSharedMemInCache(SharedMemory* mem) {
  // simple algorithm:
  //  - look for an empty slot to store mem, or
  //  - if full, then replace any existing cache entry that is smaller than the
  //    given shared memory object.
  for (int i = 0; i < arraysize(shared_mem_cache_); ++i) {
    if (!shared_mem_cache_[i]) {
      shared_mem_cache_[i] = mem;
      return true;
    }
  }
  for (int i = 0; i < arraysize(shared_mem_cache_); ++i) {
    SharedMemory* cached_mem = shared_mem_cache_[i];
    if (cached_mem->max_size() < mem->max_size()) {
      shared_mem_cache_[i] = mem;
      DeleteSharedMem(cached_mem);
      return true;
    }
  }
  return false;
}

void RenderProcess::ClearSharedMemCache() {
  for (int i = 0; i < arraysize(shared_mem_cache_); ++i) {
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
