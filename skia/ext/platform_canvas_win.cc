// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>
#include <psapi.h>

#include "skia/ext/bitmap_platform_device_win.h"
#include "skia/ext/platform_canvas.h"

namespace skia {

// Crash on failure.
#define CHECK(condition) if (!(condition)) __debugbreak();

// Crashes the process. This is called when a bitmap allocation fails, and this
// function tries to determine why it might have failed, and crash on different
// lines. This allows us to see in crash dumps the most likely reason for the
// failure. It takes the size of the bitmap we were trying to allocate as its
// arguments so we can check that as well.
//
// Note that in a sandboxed renderer this function crashes when trying to
// call GetProcessMemoryInfo() because it tries to load psapi.dll, which
// is fine but gives you a very hard to read crash dump.
__declspec(noinline) void CrashForBitmapAllocationFailure(int w, int h) {
  // If the bitmap is ginormous, then we probably can't allocate it.
  // We use 64M pixels = 256MB @ 4 bytes per pixel.
  const __int64 kGinormousBitmapPxl = 64000000;
  CHECK(static_cast<__int64>(w) * static_cast<__int64>(h) <
        kGinormousBitmapPxl);

  // The maximum number of GDI objects per process is 10K. If we're very close
  // to that, it's probably the problem.
  const int kLotsOfGDIObjs = 9990;
  CHECK(GetGuiResources(GetCurrentProcess(), GR_GDIOBJECTS) < kLotsOfGDIObjs);

  // If we're using a crazy amount of virtual address space, then maybe there
  // isn't enough for our bitmap.
  const __int64 kLotsOfMem = 1500000000;  // 1.5GB.
  PROCESS_MEMORY_COUNTERS pmc;
  if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc)))
    CHECK(pmc.PagefileUsage < kLotsOfMem);

  // Everything else.
  CHECK(0);
}

// Crashes the process. This is called when a bitmap allocation fails but
// unlike its cousin CrashForBitmapAllocationFailure() it tries to detect if
// the issue was a non-valid shared bitmap handle. 
__declspec(noinline) void CrashIfInvalidSection(HANDLE shared_section) {
  DWORD handle_info = 0;
  CHECK(::GetHandleInformation(shared_section, &handle_info) == TRUE);
}

PlatformCanvas::PlatformCanvas() : SkCanvas() {
}

PlatformCanvas::PlatformCanvas(int width, int height, bool is_opaque)
    : SkCanvas() {
  bool initialized = initialize(width, height, is_opaque, NULL);
  if (!initialized)
    CrashForBitmapAllocationFailure(width, height);
}

PlatformCanvas::PlatformCanvas(int width,
                               int height,
                               bool is_opaque,
                               HANDLE shared_section)
    : SkCanvas() {
  bool initialized = initialize(width, height, is_opaque, shared_section);
  if (!initialized) {
    CrashIfInvalidSection(shared_section);
    CrashForBitmapAllocationFailure(width, height);
  }
}

PlatformCanvas::~PlatformCanvas() {
}

bool PlatformCanvas::initialize(int width,
                               int height,
                               bool is_opaque,
                               HANDLE shared_section) {
  SkDevice* device = BitmapPlatformDevice::create(width, height,
                                                  is_opaque, shared_section);
  if (!device)
    return false;

  setDevice(device);
  device->unref();  // was created with refcount 1, and setDevice also refs
  return true;
}

HDC PlatformCanvas::beginPlatformPaint() {
  return getTopPlatformDevice().getBitmapDC();
}

void PlatformCanvas::endPlatformPaint() {
  // we don't clear the DC here since it will be likely to be used again
  // flushing will be done in onAccessBitmap
}

SkDevice* PlatformCanvas::createDevice(SkBitmap::Config config,
                                       int width,
                                       int height,
                                       bool is_opaque, bool isForLayer) {
  SkASSERT(config == SkBitmap::kARGB_8888_Config);
  return BitmapPlatformDevice::create(width, height, is_opaque, NULL);
}

}  // namespace skia
