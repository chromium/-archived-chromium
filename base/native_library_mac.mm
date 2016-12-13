// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/native_library.h"

#include <dlfcn.h>
#import <Carbon/Carbon.h>

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/scoped_cftyperef.h"
#include "base/string_util.h"

namespace base {

// static
NativeLibrary LoadNativeLibrary(const FilePath& library_path) {
  if (library_path.Extension() == "dylib" ||
      !file_util::DirectoryExists(library_path)) {
    void* dylib = dlopen(library_path.value().c_str(), RTLD_LAZY);
    if (!dylib)
      return NULL;
    NativeLibrary native_lib = new NativeLibraryStruct();
    native_lib->type = DYNAMIC_LIB;
    native_lib->dylib = dylib;
    return native_lib;
  }
  scoped_cftyperef<CFURLRef> url(CFURLCreateFromFileSystemRepresentation(
      kCFAllocatorDefault,
      (const UInt8*)library_path.value().c_str(),
      library_path.value().length(),
      true));
  if (!url)
    return NULL;
  CFBundleRef bundle = CFBundleCreate(kCFAllocatorDefault, url.get());
  if (!bundle)
    return NULL;

  NativeLibrary native_lib = new NativeLibraryStruct();
  native_lib->type = BUNDLE;
  native_lib->bundle = bundle;
  return native_lib;
}

// static
void UnloadNativeLibrary(NativeLibrary library) {
  if (library->type == BUNDLE)
    CFRelease(library->bundle);
  else
    dlclose(library->dylib);
  delete library;
}

// static
void* GetFunctionPointerFromNativeLibrary(NativeLibrary library,
                                          const char* name) {
  if (library->type == BUNDLE)
    return CFBundleGetFunctionPointerForName(library->bundle,
        CFStringCreateWithCString(kCFAllocatorDefault, name,
                                  kCFStringEncodingUTF8));

  return dlsym(library->dylib, name);
}

// static
string16 GetNativeLibraryName(const string16& name) {
  return name + ASCIIToUTF16(".dylib");
}

}  // namespace base
