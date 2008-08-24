// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/disk_cache/os_file.h"

#include "base/logging.h"

namespace disk_cache {

OSFile CreateOSFile(const std::wstring& name, int flags, bool* created) {
  DWORD disposition = 0;

  if (flags & OS_FILE_OPEN)
    disposition = OPEN_EXISTING;

  if (flags & OS_FILE_CREATE) {
    DCHECK(!disposition);
    disposition = CREATE_NEW;
  }

  if (flags & OS_FILE_OPEN_ALWAYS) {
    DCHECK(!disposition);
    disposition = OPEN_ALWAYS;
  }

  if (flags & OS_FILE_CREATE_ALWAYS) {
    DCHECK(!disposition);
    disposition = CREATE_ALWAYS;
  }

  if (!disposition) {
    NOTREACHED();
    return NULL;
  }

  DWORD access = (flags & OS_FILE_READ) ? GENERIC_READ : 0;
  if (flags & OS_FILE_WRITE)
    access |= GENERIC_WRITE;

  DWORD sharing = (flags & OS_FILE_SHARE_READ) ? FILE_SHARE_READ : 0;
  if (flags & OS_FILE_SHARE_WRITE)
    access |= FILE_SHARE_WRITE;

  HANDLE file = CreateFile(name.c_str(), access, sharing, NULL, disposition, 0,
                           NULL);

  if ((flags & OS_FILE_OPEN_ALWAYS) && created &&
      INVALID_HANDLE_VALUE != file) {
    *created = (ERROR_ALREADY_EXISTS != GetLastError());
  }

  return file;
}

}  // namespace disk_cache

