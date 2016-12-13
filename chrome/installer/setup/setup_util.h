// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file declares util functions for setup project.

#ifndef CHROME_INSTALLER_SETUP_SETUP_UTIL_H_
#define CHROME_INSTALLER_SETUP_SETUP_UTIL_H_

#include "chrome/installer/util/version.h"

namespace setup_util {
  // Find the version of Chrome from an install source directory.
  // Chrome_path should contain a version folder.
  // Returns the first version found or NULL if no version is found.
  installer::Version* GetVersionFromDir(const std::wstring& chrome_path);
}  // namespace setup_util

#endif  // CHROME_INSTALLER_SETUP_SETUP_UTIL_H_
