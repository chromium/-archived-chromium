// Copyright (c) 2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_GFX_NATIVE_WIDGET_TYPES_H_
#define BASE_GFX_NATIVE_WIDGET_TYPES_H_

#include "build/build_config.h"

// This file provides cross platform typedefs for native widget types.
//   NativeWindow: this is a handle to a native, top-level window
//   NativeView: this is a handle to a native UI element. It may be the
//     same type as a NativeWindow on some platforms.
//   NativeEditView: a handle to a native edit-box. The Mac folks wanted
//     this specific typedef.
//
// The name 'View' here meshes with OS X where the UI elements are called
// 'views' and with our Chrome UI code where the elements are also called
// 'views'.

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
#elif defined(OS_LINUX)
typedef struct _GtkWidget GtkWidget;
#endif

namespace gfx {

#if defined(OS_WIN)
typedef HWND NativeView;
typedef HWND NativeWindow;
typedef HWND NativeEditView;
#elif defined(OS_MACOSX)
typedef NSView* NativeView;
typedef NSWindow* NativeWindow;
typedef NSTextField* NativeEditView;
#elif defined(OS_LINUX)
typedef GtkWidget* NativeView;
typedef GtkWidget* NativeWindow;
typedef GtkWidget* NativeEditView;
#else  // null port.
#error No known OS defined
#endif

}  // namespace gfx

#endif  // BASE_GFX_NATIVE_WIDGET_TYPES_H_
