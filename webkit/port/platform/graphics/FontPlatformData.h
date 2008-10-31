// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FontPlatformData_h
#define FontPlatformData_h

#include "config.h"
#include "build/build_config.h"

#if defined(OS_WIN)
#include "FontPlatformDataWin.h"
#elif defined(OS_LINUX)
#include "FontPlatformDataLinux.h"
#elif defined(OS_MACOSX)
// TODO(port): This #include isn't strictly kosher, but we're currently using
// the Mac font code from upstream WebKit, and we need to pick up their header.
#undef FontPlatformData_h
#include "third_party/WebKit/WebCore/platform/graphics/mac/FontPlatformData.h"
#endif

#endif  // ifndef FontPlatformData_h
