// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_NATIVE_LIBRARY_H_
#define BASE_NATIVE_LIBRARY_H_

// This file defines a cross-platform "NativeLibrary" type which represents
// a loadable module.

#include "build/build_config.h"

#if defined(OS_WIN)
#include <windows.h>
#elif defined(OS_MACOSX)
#import <Carbon/Carbon.h>
#endif  // OS_*

#include "base/string16.h"

// Macro usefull for writing cross-platform function pointers.
#if defined(OS_WIN) && !defined(CDECL)
#define CDECL __cdecl
#else
#define CDECL
#endif

class FilePath;

namespace base {

#if defined(OS_WIN)
typedef HMODULE NativeLibrary;
#elif defined(OS_MACOSX)
enum NativeLibraryType {
  BUNDLE,
  DYNAMIC_LIB
};
struct NativeLibraryStruct {
  NativeLibraryType type;
  union {
    CFBundleRef bundle;
    void* dylib;
  };
};
typedef NativeLibraryStruct* NativeLibrary;
#elif defined(OS_LINUX)
typedef void* NativeLibrary;
#endif  // OS_*

// Loads a native library from disk.  Release it with UnloadNativeLibrary when
// you're done.
NativeLibrary LoadNativeLibrary(const FilePath& library_path);

// Unloads a native library.
void UnloadNativeLibrary(NativeLibrary library);

// Gets a function pointer from a native library.
void* GetFunctionPointerFromNativeLibrary(NativeLibrary library,
                                          const char* name);

// Returns the full platform specific name for a native library.
// For example:
// "mylib" returns "mylib.dll" on Windows, "libmylib.so" on Linux,
// "mylib.dylib" on Mac.
string16 GetNativeLibraryName(const string16& name);

}  // namespace base

#endif  // BASE_NATIVE_LIBRARY_H_
