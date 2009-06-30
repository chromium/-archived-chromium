// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_INSTALLER_UTIL_VERSION_H_
#define CHROME_INSTALLER_UTIL_VERSION_H_

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
  static Version* GetVersionFromString(const std::wstring& version_str);

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
