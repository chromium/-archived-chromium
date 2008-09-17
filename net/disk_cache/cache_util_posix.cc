// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/disk_cache/cache_util.h"

#include <errno.h>
#include <sys/statvfs.h>
#include <unistd.h>

#include "base/file_util.h"
#include "base/logging.h"
#include "base/string_util.h"

#if defined(OS_MACOSX)
#include <mach/mach_host.h>
#include <mach/mach_init.h>
#endif

namespace disk_cache {

int64 GetFreeDiskSpace(const std::wstring& path) {
  struct statvfs stats;
  if (statvfs(WideToUTF8(path).c_str(), &stats) != 0) {
    return -1;
  }
  return static_cast<int64>(stats.f_bavail) * stats.f_frsize;
}

bool MoveCache(const std::wstring& from_path, const std::wstring& to_path) {
  // Just use the version from base.
  return file_util::Move(from_path.c_str(), to_path.c_str());
}

void DeleteCache(const std::wstring& path, bool remove_folder) {
  if (remove_folder) {
    file_util::Delete(path, false);
  } else {
    std::wstring name(path);
    // TODO(rvargas): Fix this after file_util::delete is fixed.
    // file_util::AppendToPath(&name, L"*");
    file_util::Delete(name, true);
  }
}

bool DeleteCacheFile(const std::wstring& name) {
  return file_util::Delete(name, false);
}

void WaitForPendingIO(int* num_pending_io) {
  if (*num_pending_io) {
    NOTIMPLEMENTED();
  }
}

}  // namespace disk_cache

