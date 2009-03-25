// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file extends generic BrowserDistribution class to declare Google Chrome
// specific implementation.

#ifndef CHROME_INSTALLER_UTIL_GOOGLE_CHROME_DISTRIBUTION_H_
#define CHROME_INSTALLER_UTIL_GOOGLE_CHROME_DISTRIBUTION_H_

#include "chrome/installer/util/browser_distribution.h"
#include "chrome/installer/util/util_constants.h"

#include "testing/gtest/include/gtest/gtest_prod.h"  // for FRIEND_TEST

class DictionaryValue;

class GoogleChromeDistribution : public BrowserDistribution {
 public:
  virtual void DoPostUninstallOperations(const installer::Version& version,
                                         const std::wstring& local_data_path);

  virtual std::wstring GetApplicationName();

  virtual std::wstring GetAlternateApplicationName();

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

  virtual std::wstring GetAppDescription();

  virtual int GetInstallReturnCode(
      installer_util::InstallStatus install_status);

  virtual std::wstring GetStateKey();

  virtual std::wstring GetUninstallLinkName();

  virtual std::wstring GetUninstallRegPath();

  virtual std::wstring GetVersionKey();

  virtual void UpdateDiffInstallStatus(bool system_install,
      bool incremental_install, installer_util::InstallStatus install_status);

 private:
  friend class BrowserDistribution;
  FRIEND_TEST(GoogleChromeDistributionTest, TestExtractUninstallMetrics);

  // Extracts uninstall metrics from the JSON file located at file_path.
  // Returns them in a form suitable for appending to a url that already
  // has GET parameters, i.e. &metric1=foo&metric2=bar.
  // Returns true if uninstall_metrics has been successfully populated with
  // the uninstall metrics, false otherwise.
  virtual bool ExtractUninstallMetricsFromFile(
      const std::wstring& file_path, std::wstring* uninstall_metrics);

  // Extracts uninstall metrics from the given JSON value.
  virtual bool ExtractUninstallMetrics(const DictionaryValue& root,
      std::wstring* uninstall_metrics);

  // Given a DictionaryValue containing a set of uninstall metrics,
  // this builds a URL parameter list of all the contained metrics.
  // Returns true if at least one uninstall metric was found in
  // uninstall_metrics_dict, false otherwise.
  virtual bool BuildUninstallMetricsString(
      DictionaryValue* uninstall_metrics_dict, std::wstring* metrics);

  // Disallow construction from non-friends.
  GoogleChromeDistribution() {}
};

#endif  // CHROME_INSTALLER_UTIL_GOOGLE_CHROME_DISTRIBUTION_H_
