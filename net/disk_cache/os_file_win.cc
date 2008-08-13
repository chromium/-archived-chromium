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
