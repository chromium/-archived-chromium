/*
 * Copyright 2009, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


// This file implements a failure handler for the new
// operator and malloc.

// TODO: This does not currently work on linux. The replacement
// operator new, malloc, etc do not take priority over those declared in
// the standard libraries.

#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>
#include <build/build_config.h>

#ifdef _MSC_VER
#include <new.h>
#endif

#ifdef OS_WIN
#include <windows.h>
#endif

#if defined(OS_MACOSX) || defined(OS_LINUX)
#include <dlfcn.h>
#endif

#include "plugin/cross/out_of_memory.h"
#include "plugin/cross/plugin_metrics.h"

#ifdef _MSC_VER
extern "C" {
  void* _getptd();
}  // _MSC_VER
#endif

namespace o3d {
namespace {
// The reserve is allocated just after the plugin starts.
// In the event that an allocation fails, the reserve is
// freed, hopefully freeing enough memory to allow any code
// run after abort() to do its work.
const size_t kReserveSize = 1024 * 256;
void* g_reserve;
}  // namespace anonymous

// This is called when a memory allocation fails in the plugin. Note
// that it will not be called if a memory allocation fails in another
// shared library, such as in the c runtime library on platforms where
// we use a shared c runtime library. In those cases, we have to hope
// that the allocation failed because new was called (in which case an
// exception will be thrown but the reserve will not be freed) or that
// the calling library correctly checks for a NULL return and does
// something appropriate.
int HandleOutOfMemory(size_t size) {
  if (g_reserve != NULL) {
    // First time round, free the reserve and exit with abort.
    // This should allow some crash reporting before the process
    // exits.signal
    free(g_reserve);
    g_reserve = NULL;

    // Do this on MacOSX and Linux when they support metrics.
    // Also, at the time of writing, the matrics logging is not
    // hooked up to breakpad, so this metric will not get logged
    // anywhere! Remove this comment when that is done and tested.
#ifdef OS_WIN
    ++metric_out_of_memory_total;
#endif  // OS_MACOSX

    fprintf(stderr, "Aborting: out of memory allocating %lu bytes\n",
            static_cast<unsigned long>(size));

#ifdef OS_WIN
    // This is different on Windows so that it is comnpatible with
    // the way that breakpad works. On Windows, it intercepts
    // exceptions. On unixy platforms, it handles signals. Also,
    // on Windows, this is friendlier to the browser's own crash
    // logging (for browsers that log crashes).
    RaiseException(ERROR_OUTOFMEMORY,
                   EXCEPTION_NONCONTINUABLE_EXCEPTION,
                   0, NULL);
#else  // OS_WIN
    abort();
#endif  // OS_WIN
  } else {
    // If the handler is reentered, try to exit without raising
    // SIGABRT (or executing exit handlers).
    _exit(EXIT_FAILURE);
  }
  return 0;
}

bool SetupOutOfMemoryHandler() {
#ifdef _MSC_VER
  // Workaround for MSVC. Sometimes the runtime calls this when
  // there is not enough memory to allocate the "per-thread
  // data structure". Calling this forces the "per-thread data
  // structure" to be allocated.
  _getptd();

  // This causes malloc failures to call the new handler as well.
  // It is not necessary to replace the implementations of malloc,
  // etc under MSVC.
  _set_new_mode(1);
  _set_new_handler(HandleOutOfMemory);
#endif  // _MSC_VER

  g_reserve = malloc(kReserveSize);

  return true;
}
}  // namespace o3d

#if defined(OS_MACOSX) || defined(OS_LINUX)
namespace {
void* dlsym_helper(const char* symbol_name) {
  void* ptr = dlsym(RTLD_NEXT, symbol_name);
  if (ptr == NULL) {
    fprintf(stderr, "Error: could not locate symbol \"%s\"\n", symbol_name);
    abort();
  }
  return ptr;
}
}  // namespace anonymous

void* operator new(size_t size) {
  return malloc(size);
}

void operator delete(void* ptr) {
  free(ptr);
}

extern "C" {
void *malloc(size_t size) {
  typedef void* (*Func)(size_t);
  static Func func = (Func) dlsym_helper("malloc");
  void* ptr = func(size);
  if (ptr == NULL)
    o3d::HandleOutOfMemory(size);
  return ptr;
}

void *realloc(void* old_ptr, size_t new_size) {
  typedef void* (*Func)(void*, size_t);
  static Func func = reinterpret_cast<Func>(dlsym_helper("realloc"));
  void* ptr = func(old_ptr, new_size);

  // realloc() returns NULL when you ask for zero bytes. This differs
  // from malloc() which returns a pointer to a zero sized allocated block.
  if (new_size != 0 && ptr == NULL)
    o3d::HandleOutOfMemory(new_size);
  return ptr;
}

void *calloc(size_t num_items, size_t size) {
  typedef void* (*Func)(size_t, size_t);
  static Func func = reinterpret_cast<Func>(dlsym_helper("calloc"));
  void* ptr = func(num_items, size);
  if (ptr == NULL)
    o3d::HandleOutOfMemory(size);
  return ptr;
}

void *valloc(size_t size) {
  typedef void* (*Func)(size_t);
  static Func func = reinterpret_cast<Func>(dlsym_helper("valloc"));
  void* ptr = func(size);
  if (ptr == NULL)
    o3d::HandleOutOfMemory(size);
  return ptr;
}

void* memalign(size_t alignment, size_t size) {
  typedef void* (*Func)(size_t, size_t);
  static Func func = reinterpret_cast<Func>(dlsym_helper("memalign"));
  void* ptr = func(alignment, size);
  if (ptr == NULL)
    o3d::HandleOutOfMemory(size);
  return ptr;
}

char* strdup(const char* ptr) {
  typedef char* (*Func)(const char*);
  static Func func = reinterpret_cast<Func>(dlsym_helper("strdup"));
  char* result = func(ptr);
  if (ptr != NULL && result == NULL)
    o3d::HandleOutOfMemory((strlen(ptr) + 1) * sizeof(char));
  return result;
}

wchar_t* wcsdup(const wchar_t* ptr) {
  typedef wchar_t* (*Func)(const wchar_t*);
  static Func func = reinterpret_cast<Func>(dlsym_helper("wcsdup"));
  wchar_t* result = func(ptr);
  if (ptr != NULL && result == NULL)
    o3d::HandleOutOfMemory((wcslen(ptr) + 1) * sizeof(wchar_t));
  return result;
}
}
#endif  // defined(OS_MACOSX) || defined(OS_LINUX)
