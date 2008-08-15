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

#ifndef NET_DISK_CACHE_CACHE_UTIL_H_
#define NET_DISK_CACHE_CACHE_UTIL_H_

#include <string>

#include "base/basictypes.h"

namespace disk_cache {

// Returns the available disk space on the volume that contains |path|, or -1
// on failure.
int64 GetFreeDiskSpace(const std::wstring& path);

// Returns the total physical memory on the system, or -1 on failure.
int64 GetSystemMemory();

// Moves the cache files from the given path to another location.
// Returns true if successful, false otherwise.
bool MoveCache(const std::wstring& from_path, const std::wstring& to_path);

// Deletes the cache files stored on |path|, and optionally also attempts to
// delete the folder itself.
void DeleteCache(const std::wstring& path, bool remove_folder);

// Deletes a cache file.
bool DeleteCacheFile(const std::wstring& name);

// Blocks until |num_pending_io| IO operations complete.
void WaitForPendingIO(int num_pending_io);

}  // namespace disk_cache

#endif  // NET_DISK_CACHE_CACHE_UTIL_H_
