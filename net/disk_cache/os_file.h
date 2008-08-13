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
