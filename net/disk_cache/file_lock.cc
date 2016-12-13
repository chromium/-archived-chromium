// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/disk_cache/file_lock.h"

namespace disk_cache {

FileLock::FileLock(BlockFileHeader* header) {
  updating_ = &header->updating;
  (*updating_)++;
  acquired_ = true;
}

void FileLock::Lock() {
  if (acquired_)
    return;
  (*updating_)++;
}

void FileLock::Unlock() {
  if (!acquired_)
    return;
  (*updating_)--;
}

}  // namespace disk_cache
