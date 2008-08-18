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
// This file extends generic BrowserDistribution class to declare Google Chrome
// specific implementation.

#ifndef CHROME_INSTALLER_UTIL_GOOGLE_CHROME_DISTRIBUTION_H_
#define CHROME_INSTALLER_UTIL_GOOGLE_CHROME_DISTRIBUTION_H_

#include "chrome/installer/util/browser_distribution.h"
#include "chrome/installer/util/util_constants.h"

class GoogleChromeDistribution : public BrowserDistribution {
 public:
  virtual void DoPostUninstallOperations(const installer::Version& version);

  virtual void DoPreUninstallOperations();

  virtual std::wstring GetApplicationName();

  virtual std::wstring GetInstallSubDir();

  // This method generates the new value for Google Update "ap" key for Chrome
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
  std::wstring GetNewGoogleUpdateApKey(bool diff_install,
      installer_util::InstallStatus status, const std::wstring& value);

  virtual std::wstring GetPublisherName();

  virtual int GetInstallReturnCode(
      installer_util::InstallStatus install_status);

  virtual std::wstring GetUninstallRegPath();

  virtual std::wstring GetVersionKey();

  virtual void UpdateDiffInstallStatus(bool system_install,
      bool incremental_install, installer_util::InstallStatus install_status);

 private:
  friend class BrowserDistribution;

  GoogleChromeDistribution() {}
};

#endif  // CHROME_INSTALLER_UTIL_GOOGLE_CHROME_DISTRIBUTION_H_
