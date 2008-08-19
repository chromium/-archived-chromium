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
