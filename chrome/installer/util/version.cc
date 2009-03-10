// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/string_util.h"
#include "chrome/installer/util/version.h"

installer::Version::Version(int64 major, int64 minor, int64 build,
                            int64 patch) :
    major_(major), minor_(minor), build_(build), patch_(patch) {
  version_str_.append(Int64ToWString(major_));
  version_str_.append(L".");
  version_str_.append(Int64ToWString(minor_));
  version_str_.append(L".");
  version_str_.append(Int64ToWString(build_));
  version_str_.append(L".");
  version_str_.append(Int64ToWString(patch_));
}

installer::Version::~Version() {
}

bool installer::Version::IsHigherThan(const installer::Version* other) const {
  return ((major_ > other->major_) ||
          ((major_ == other->major_) && (minor_ > other->minor_)) ||
          ((major_ == other->major_) && (minor_ == other->minor_)
                                     && (build_ > other->build_)) ||
          ((major_ == other->major_) && (minor_ == other->minor_)
                                     && (build_ == other->build_)
                                     && (patch_ > other->patch_)));
}

installer::Version* installer::Version::GetVersionFromString(
    std::wstring version_str) {
  std::vector<std::wstring> numbers;
  SplitString(version_str, '.', &numbers);

  if (numbers.size() != 4) {
    return NULL;
  }

  return new Version(StringToInt64(numbers[0]), StringToInt64(numbers[1]),
                     StringToInt64(numbers[2]), StringToInt64(numbers[3]));
}
