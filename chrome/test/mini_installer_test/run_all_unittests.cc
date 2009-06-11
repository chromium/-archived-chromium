// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/file_util.h"
#include "base/test_suite.h"
#include "chrome/test/mini_installer_test/mini_installer_test_constants.h"
#include "chrome_mini_installer.h"

void BackUpProfile() {
  if (base::GetProcessCount(L"chrome.exe", NULL) > 0) {
    printf("Chrome is currently running and cannot backup the profile."
           "Please close Chrome and run the tests again.\n");
    exit(1);
  }
  ChromeMiniInstaller installer(mini_installer_constants::kUserInstall,
                                mini_installer_constants::kDevChannelBuild);
  std::wstring path = installer.GetChromeInstallDirectoryLocation();
  file_util::AppendToPath(&path, mini_installer_constants::kChromeAppDir);
  file_util::UpOneDirectory(&path);
  std::wstring backup_path = path;
  // Will hold User Data path that needs to be backed-up.
  file_util::AppendToPath(&path,
      mini_installer_constants::kChromeUserDataDir);
  // Will hold new backup path to save the profile.
  file_util::AppendToPath(&backup_path,
      mini_installer_constants::kChromeUserDataBackupDir);
  // Will check if User Data profile is available.
  if (file_util::PathExists(path)) {
    // Will check if User Data is already backed up.
    // If yes, will delete and create new one.
    if (file_util::PathExists(backup_path))
      file_util::Delete(backup_path.c_str(), true);
    file_util::CopyDirectory(path, backup_path, true);
  } else {
    printf("Chrome is not installed. Will not take any backup\n");
  }
}

int main(int argc, char** argv) {
  // Check command line to decide if the tests should continue
  // with cleaning the system or make a backup before continuing.
  CommandLine::Init(argc, argv);
  const CommandLine& command_line = *CommandLine::ForCurrentProcess();
  if (command_line.HasSwitch(L"clean")) {
    printf("Current version of Chrome will be uninstalled "
           "from all levels before proceeding with tests.\n");
  } else if (command_line.HasSwitch(L"backup")) {
    BackUpProfile();
  } else {
    printf("This test needs command line Arguments.\n");
    printf("Usage: mini_installer_tests.exe -{clean|backup}\n");
    printf("Note: -clean arg will uninstall your chrome at all levels"
           " and also delete profile.\n"
           "-backup arg will make a copy of User Data before uninstalling"
           " your chrome at all levels. The copy will be named as"
           " User Data Copy.\n");
    exit(1);
  }
  return TestSuite(argc, argv).Run();
}
