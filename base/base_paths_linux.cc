// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/base_paths_linux.h"

#include <unistd.h>

#include "base/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/string_piece.h"
#include "base/sys_string_conversions.h"

namespace base {

bool PathProviderLinux(int key, std::wstring* result) {
  std::wstring cur;
  switch (key) {
    case base::FILE_EXE:
    case base::FILE_MODULE: { // TODO(evanm): is this correct?
      char bin_dir[PATH_MAX + 1];
      int bin_dir_size = readlink("/proc/self/exe", bin_dir, PATH_MAX);
      if (bin_dir_size < 0 || bin_dir_size > PATH_MAX) {
        NOTREACHED() << "Unable to resolve /proc/self/exe.";
        return false;
      }
      bin_dir[bin_dir_size] = 0;
      *result = base::SysNativeMBToWide(bin_dir);
      return true;
    }
    case base::DIR_SOURCE_ROOT:
      // On linux, unit tests execute two levels deep from the source root.
      // For example:  chrome/{Debug|Hammer}/net_unittest
      PathService::Get(base::DIR_EXE, &cur);
      file_util::UpOneDirectory(&cur);
      file_util::UpOneDirectory(&cur);
      *result = cur;
      return true;
  }
  return false;
}

}  // namespace base

