// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_util.h"

#import <Cocoa/Cocoa.h>
#include <copyfile.h>

#include "base/logging.h"
#include "base/string_util.h"

namespace file_util {

const wchar_t kPathSeparator = L'/';

bool GetTempDir(std::wstring* path) {
  NSString* tmp = NSTemporaryDirectory();
  if (tmp == nil)
    return false;
  *path = reinterpret_cast<const wchar_t*>(
      [tmp cStringUsingEncoding:NSUTF32StringEncoding]);
  return true;
}

bool CopyFile(const std::wstring& from_path, const std::wstring& to_path) {
  return (copyfile(WideToUTF8(from_path).c_str(),
                   WideToUTF8(to_path).c_str(), NULL, COPYFILE_ALL) == 0);
}

}  // namespace
