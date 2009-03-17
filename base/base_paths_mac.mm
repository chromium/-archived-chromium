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
  std::string cur;
  switch (key) {
    case base::FILE_EXE:
    case base::FILE_MODULE: {
      // Executable path can have relative references ("..") depending on
      // how the app was launched.
      NSString* path =
          [[[NSBundle mainBundle] executablePath] stringByStandardizingPath];
      cur = [path fileSystemRepresentation];
      break;
    }
    case base::DIR_SOURCE_ROOT: {
      FilePath path;
      // On the mac, unit tests execute two levels deep from the source root.
      // For example: src/xcodebuild/{Debug|Release}/base_unittests
      PathService::Get(base::DIR_EXE, &path);
      path = path.DirName();
      *result = path.DirName();
      return true;
    }
    default:
      return false;
  }

  *result = FilePath(cur);
  return true;
}

}  // namespace base
