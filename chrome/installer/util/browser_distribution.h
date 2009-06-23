// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file declares a class that contains various method related to branding.

#ifndef CHROME_INSTALLER_UTIL_BROWSER_DISTRIBUTION_H_
#define CHROME_INSTALLER_UTIL_BROWSER_DISTRIBUTION_H_

#include "base/basictypes.h"
#include "chrome/installer/util/util_constants.h"
#include "chrome/installer/util/version.h"

class RegKey;

class BrowserDistribution {
 public:
  virtual ~BrowserDistribution() {}

  static BrowserDistribution* GetDistribution();

  virtual void DoPostUninstallOperations(const installer::Version& version,
                                         const std::wstring& local_data_path,
                                         const std::wstring& distribution_data);

  virtual std::wstring GetApplicationName();

  virtual std::wstring GetAlternateApplicationName();

  virtual std::wstring GetInstallSubDir();

  virtual std::wstring GetPublisherName();

  virtual std::wstring GetAppDescription();

  virtual int GetInstallReturnCode(
      installer_util::InstallStatus install_status);

  virtual std::wstring GetStateKey();

  virtual std::wstring GetStateMediumKey();

  virtual std::wstring GetStatsServerURL();

  virtual std::wstring GetDistributionData(RegKey* key);

  virtual std::wstring GetUninstallLinkName();

  virtual std::wstring GetUninstallRegPath();

  virtual std::wstring GetVersionKey();

  virtual void UpdateDiffInstallStatus(bool system_install,
      bool incremental_install, installer_util::InstallStatus install_status);

  // After an install or upgrade the user might qualify to participate in an
  // experiment. This function determines if the user qualifies and if so it
  // sets the wheels in motion or in simple cases does the experiment itself.
  virtual void LaunchUserExperiment(installer_util::InstallStatus status,
                                    const installer::Version& version,
                                    bool system_install, int options);

  // The user has qualified for the inactive user toast experiment and this
  // function just performs it.
  virtual void InactiveUserToastExperiment();

 protected:
  BrowserDistribution() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(BrowserDistribution);
};

#endif  // CHROME_INSTALLER_UTIL_BROWSER_DISTRIBUTION_H_
