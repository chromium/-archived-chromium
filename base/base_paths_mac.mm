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

#include "base/base_paths_mac.h"

#import <Cocoa/Cocoa.h>

#include "base/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/string_util.h"

namespace base {

bool PathProviderMac(int key, std::wstring* result) {
  std::wstring cur;
  switch (key) {
    case base::FILE_EXE:
    case base::FILE_MODULE: {
      NSString* path = [[NSBundle mainBundle] executablePath];
      cur = reinterpret_cast<const wchar_t*>(
          [path cStringUsingEncoding:NSUTF32StringEncoding]);
      break;
    }
    case base::DIR_APP_DATA:
    case base::DIR_LOCAL_APP_DATA: {
      // TODO(erikkay): maybe we should remove one of these for mac?  The local
      // vs. roaming distinction is fairly Windows-specific.
      NSArray* dirs = NSSearchPathForDirectoriesInDomains(
          NSApplicationSupportDirectory, NSUserDomainMask, YES);
      if (!dirs || [dirs count] == 0)
        return false;
      DCHECK([dirs count] == 1);
      NSString* tail = [[NSString alloc] initWithCString:"Google/Chrome"];
      NSString* path = [[dirs lastObject] stringByAppendingPathComponent:tail];
      cur = reinterpret_cast<const wchar_t*>(
          [path cStringUsingEncoding:NSUTF32StringEncoding]);
      break;
    }
    case base::DIR_SOURCE_ROOT:
      // On the mac, unit tests execute three levels deep from the source root.
      // For example:  chrome/build/{Debug|Release}/ui_tests
      PathService::Get(base::DIR_EXE, &cur);
      file_util::UpOneDirectory(&cur);
      file_util::UpOneDirectory(&cur);
      file_util::UpOneDirectory(&cur);
      break;
    default:
      return false;
  }

  result->swap(cur);
  return true;
}

}  // namespace base
