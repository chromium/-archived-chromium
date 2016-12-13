// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SKIA_EXT_PLATFORM_DEVICE_H_
#define SKIA_EXT_PLATFORM_DEVICE_H_

// This file provides an easy way to include the appropriate PlatformDevice
// header file for your platform.

#if defined(WIN32)
#include "skia/ext/platform_device_win.h"
#elif defined(__APPLE__)
#include "skia/ext/platform_device_mac.h"
#elif defined(__linux__)
#include "skia/ext/platform_device_linux.h"
#endif

#endif
