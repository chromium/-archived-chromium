// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file defines a class that contains various method related to branding.
// It provides only default implementations of these methods. Usually to add
// specific branding, we will need to extend this class with a custom
// implementation.

#include "chrome/installer/util/browser_distribution.h"

#include "base/registry.h"
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
    const installer::Version& version, const std::wstring& local_data_path,
    const std::wstring& distribution_data) {
}

std::wstring BrowserDistribution::GetApplicationName() {
  return L"Chromium";
}

std::wstring BrowserDistribution::GetAlternateApplicationName() {
  return L"The Internet";
}

std::wstring BrowserDistribution::GetInstallSubDir() {
  return L"Chromium";
}

std::wstring BrowserDistribution::GetPublisherName() {
  return L"Chromium";
}

std::wstring BrowserDistribution::GetAppDescription() {
  return L"Browse the web";
}

int BrowserDistribution::GetInstallReturnCode(
    installer_util::InstallStatus install_status) {
  return install_status;
}

std::wstring BrowserDistribution::GetStateKey() {
  return L"Software\\Chromium";
}

std::wstring BrowserDistribution::GetStateMediumKey() {
  return L"Software\\Chromium";
}

std::wstring BrowserDistribution::GetStatsServerURL() {
  return L"";
}

std::wstring BrowserDistribution::GetDistributionData(RegKey* key) {
  return L"";
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

void BrowserDistribution::LaunchUserExperiment(
    installer_util::InstallStatus status, const installer::Version& version,
    bool system_install, int options) {
}


void BrowserDistribution::InactiveUserToastExperiment() {
}