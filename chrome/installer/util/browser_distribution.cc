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
// This file defines a class that contains various method related to branding.
// It provides only default implementations of these methods. Usually to add
// specific branding, we will need to extend this class with a custom
// implementation.

#include "chrome/installer/util/browser_distribution.h"
#include "chrome/installer/util/google_chrome_distribution.h"

BrowserDistribution* BrowserDistribution::GetDistribution() {
  static BrowserDistribution* dist = NULL;
  if (dist == NULL) {
#if defined(GOOGLE_CHROME_BUILD)
    dist = new GoogleChromeDistribution();
#else
    dist = new BrowserDistribution();
#endif
  }
  return dist;
}

void BrowserDistribution::DoPostUninstallOperations(
    const installer::Version& version) {
}

void BrowserDistribution::DoPreUninstallOperations() {
}

std::wstring BrowserDistribution::GetApplicationName() {
  return L"Chromium";
}

std::wstring BrowserDistribution::GetInstallSubDir() {
  return L"Chromium";
}

std::wstring BrowserDistribution::GetPublisherName() {
  return L"Chromium";
}

int BrowserDistribution::GetInstallReturnCode(
    installer_util::InstallStatus install_status) {
  return install_status;
}

std::wstring BrowserDistribution::GetUninstallLinkName() {
  return L"Uninstall Chromium";
}

std::wstring BrowserDistribution::GetUninstallRegPath() {
  return L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\Chromium";
}

std::wstring BrowserDistribution::GetVersionKey() {
  return L"Software\\Chromium";
}

void BrowserDistribution::UpdateDiffInstallStatus(bool system_install,
    bool incremental_install, installer_util::InstallStatus install_status) {
}
