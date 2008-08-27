// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/disk_cache/os_file.h"

#include <fcntl.h>

#include "base/logging.h"
#include "base/string_util.h"

namespace disk_cache {

OSFile CreateOSFile(const std::wstring& name, int flags, bool* created) {
  int open_flags = 0;
  if (flags & OS_FILE_CREATE)
    open_flags = O_CREAT | O_EXCL;

  if (flags & OS_FILE_CREATE_ALWAYS) {
    DCHECK(!open_flags);
    open_flags = O_CREAT | O_TRUNC;
  }

  if (!open_flags && !(flags & OS_FILE_OPEN) &&
      !(flags & OS_FILE_OPEN_ALWAYS)) {
    NOTREACHED();
    return -1;
  }

  if (flags & OS_FILE_WRITE && flags & OS_FILE_READ) {
    open_flags |= O_RDWR;
  } else if (flags & OS_FILE_WRITE) {
    open_flags |= O_WRONLY;
  } else if (!(flags & OS_FILE_READ)) {
    NOTREACHED();
  }

  DCHECK(O_RDONLY == 0);

  int descriptor = open(WideToUTF8(name).c_str(), open_flags,
                        S_IRUSR | S_IWUSR);

  if (flags & OS_FILE_OPEN_ALWAYS) {
    if (descriptor > 0) {
      if (created)
        *created = false;
    } else {
      open_flags |= O_CREAT;
      descriptor = open(WideToUTF8(name).c_str(), open_flags,
                        S_IRUSR | S_IWUSR);
      if (created && descriptor > 0)
        *created = true;
    }
  }

  return descriptor;
}

}  // namespace disk_cache

