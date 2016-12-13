// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/disk_cache/cache_util.h"

#include "base/file_util.h"
#include "base/logging.h"
#include "base/string_util.h"

namespace disk_cache {

bool MoveCache(const std::wstring& from_path, const std::wstring& to_path) {
  // Just use the version from base.
  return file_util::Move(from_path.c_str(), to_path.c_str());
}

void DeleteCache(const std::wstring& path, bool remove_folder) {
  file_util::FileEnumerator iter(FilePath::FromWStringHack(path),
                                 /* recursive */ false,
                                 file_util::FileEnumerator::FILES);
  for (FilePath file = iter.Next(); !file.value().empty(); file = iter.Next()) {
    if (!file_util::Delete(file, /* recursive */ false))
      NOTREACHED();
  }

  if (remove_folder) {
    if (!file_util::Delete(path, /* recursive */ false))
      NOTREACHED();
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
