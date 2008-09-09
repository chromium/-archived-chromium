// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DISK_CACHE_OS_FILE_H_
#define NET_DISK_CACHE_OS_FILE_H_

#include <string>

#include "build/build_config.h"

namespace disk_cache {

#if defined(OS_WIN)
#include <windows.h>
typedef HANDLE OSFile;
#elif defined(OS_POSIX)
typedef int OSFile;
const OSFile INVALID_HANDLE_VALUE = -1;
#endif

enum OSFileFlags {
  OS_FILE_OPEN = 1,
  OS_FILE_CREATE = 2,
  OS_FILE_OPEN_ALWAYS = 4,    // May create a new file.
  OS_FILE_CREATE_ALWAYS = 8,  // May overwrite an old file.
  OS_FILE_READ = 16,
  OS_FILE_WRITE = 32,
  OS_FILE_SHARE_READ = 64,
  OS_FILE_SHARE_WRITE = 128
};

// Creates or open the given file. If OS_FILE_OPEN_ALWAYS is used, and |created|
// is provided, |created| will be set to true if the file was created or to
// false in case the file was just opened.
OSFile CreateOSFile(const std::wstring& name, int flags, bool* created);

}  // namespace disk_cache

#endif  // NET_DISK_CACHE_OS_FILE_H_
