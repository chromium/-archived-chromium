// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/base_paths_mac.h"

#import <Cocoa/Cocoa.h>

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/string_util.h"

namespace base {

bool PathProviderMac(int key, FilePath* result) {
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
      // On the mac, unit tests execute two levels deep from the source root.
      // For example: src/xcodebuild/{Debug|Release}/base_unittests
      PathService::Get(base::DIR_EXE, &cur);
      file_util::UpOneDirectory(&cur);
      file_util::UpOneDirectory(&cur);
      break;
    default:
      return false;
  }

  *result = FilePath::FromWStringHack(cur);
  return true;
}

}  // namespace base
