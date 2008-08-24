// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file declares utility functions for the installer. The original reason
// for putting these functions in installer\util library is so that we can
// separate out the critical logic and write unit tests for it.

#ifndef CHROME_INSTALLER_UTIL_INSTALL_UTIL_H__
#define CHROME_INSTALLER_UTIL_INSTALL_UTIL_H__

#include <string>

#include "base/basictypes.h"
#include "chrome/installer/util/util_constants.h"
#include "chrome/installer/util/version.h"

// This is a utility class that provides common installation related
// utility methods that can be used by installer and also unit tested
// independently.
class InstallUtil {
 public:
  // Find the version of Chrome installed on the system by checking the
  // Google Update registry key. Returns the version or NULL if no version is
  // found.
  // system_install: if true, looks for version number under the HKLM root,
  //                 otherwise looks under the HKCU.
  static installer::Version * GetChromeVersion(bool system_install);
 private:
  DISALLOW_EVIL_CONSTRUCTORS(InstallUtil);
};


#endif  // CHROME_INSTALLER_UTIL_INSTALL_UTIL_H__

