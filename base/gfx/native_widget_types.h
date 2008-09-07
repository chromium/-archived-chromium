// Copyright (c) 2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_GFX_NATIVE_WIDGET_TYPES_H_
#define BASE_GFX_NATIVE_WIDGET_TYPES_H_

#include "build/build_config.h"

#if defined(OS_WIN)
#include <windows.h>
#elif defined(OS_MACOSX)
#ifdef __OBJC__
@class NSView;
@class NSWindow;
@class NSTextField;
#else
class NSView;
class NSWindow;
class NSTextField;
#endif  // __OBJC__
#endif  // MACOSX

namespace gfx {

#if defined(OS_WIN)
typedef HWND ViewHandle;
typedef HWND WindowHandle;
typedef HWND EditViewHandle;
#elif defined(OS_MACOSX)
typedef NSView *ViewHandle;
typedef NSWindow *WindowHandle;
typedef NSTextField *EditViewHandle;
#else  // null port.
typedef void* ViewHandle;
typedef void* WindowHandle;
typedef void* EditViewHandle;
#endif

}  // namespace gfx

#endif  // BASE_GFX_NATIVE_WIDGET_TYPES_H_
