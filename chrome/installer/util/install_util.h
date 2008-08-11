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
  // This method gets the Google Update registry key path for Chrome.
  // i.e. - Software\Google\Update\Clients\<chrome-guid>";
  static std::wstring GetChromeGoogleUpdateKey();

  // Find the version of Chrome installed on the system by checking the
  // Google Update registry key. Returns the version or NULL if no version is
  // found.
  // system_install: if true, looks for version number under the HKLM root,
  //                 otherwise looks under the HKCU.
  static installer::Version * GetChromeVersion(bool system_install);

  // This method generates the new value for Oamaha "ap" key for Chrome
  // based on whether we are doing incremental install (or not) and whether
  // the install succeeded.
  // - If install worked, remove the magic string (if present).
  // - If incremental installer failed, append a magic string (if
  //   not present already).
  // - If full installer failed, still remove this magic
  //   string (if it is present already).
  //
  // diff_install: tells whether this is incremental install or not.
  // install_status: if 0, means installation was successful.
  // value: current value of Google Update "ap" key.
  static std::wstring GetNewGoogleUpdateApKey(bool diff_install,
      installer_util::InstallStatus status, const std::wstring& value);

  // Given an InstallStatus it tells whether the install was sucessful or not.
  static bool InstallSuccessful(installer_util::InstallStatus status);
 private:
  DISALLOW_EVIL_CONSTRUCTORS(InstallUtil);
};


#endif  // CHROME_INSTALLER_UTIL_INSTALL_UTIL_H__
