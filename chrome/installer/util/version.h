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

#ifndef CHROME_INSTALLER_UTIL_VERSION_H__
#define CHROME_INSTALLER_UTIL_VERSION_H__

#include <string>

#include "base/basictypes.h"

namespace installer {

class Version {
public:
  virtual ~Version();

  // Check if the current version is higher than the version object passed
  // as parameter
  bool IsHigherThan(const Version* other) const;

  // Return the string representation of this version
  const std::wstring& GetString() const {
    return version_str_;
  }

  // Assume that the version string is specified by four integers separated
  // by character '.'. Return NULL if string is not of this format.
  // Caller is responsible for freeing the Version object once done.
  static Version* GetVersionFromString(std::wstring version_str);

private:
  int64 major_;
  int64 minor_;
  int64 build_;
  int64 patch_;
  std::wstring version_str_;

  // Classes outside this file do not have any need to create objects of
  // this type so declare constructor as private.
  Version(int64 major, int64 minor, int64 build, int64 patch);
};

}  // namespace installer

#endif
