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
