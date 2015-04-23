// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file contains helper functions used by setup.

#ifndef CHROME_INSTALLER_UTIL_HELPER_H__
#define CHROME_INSTALLER_UTIL_HELPER_H__

#include <string>

namespace installer {

// This function returns the install path for Chrome depending on whether its
// system wide install or user specific install.
// system_install: if true, the function returns system wide location
//                 (ProgramFiles\Google). Otherwise it returns user specific
//                 location (Document And Settings\<user>\Local Settings...)
std::wstring GetChromeInstallPath(bool system_install);

// This function returns the path to the directory that holds the user data,
// this is always inside "Document And Settings\<user>\Local Settings.". Note
// that this is the default user data directory and does not take into account
// that it can be overriden with a command line parameter.
std::wstring GetChromeUserDataPath();

// Launches Chrome without waiting for its exit.
bool LaunchChrome(bool system_install);

// Launches Chrome with given command line, waits for Chrome indefinitely
// (until it terminates), and gets the process exit code if available.
// The function returns true as long as Chrome is successfully launched.
// The status of Chrome at the return of the function is given by exit_code.
bool LaunchChromeAndWaitForResult(bool system_install,
                                  const std::wstring& options,
                                  int32* exit_code);

// This function tries to remove all previous version directories after a new
// Chrome update. If an instance of Chrome with older version is still running
// on the system, its corresponding version directory will be left intact.
// (The version directory is subject for removal again during next update.)
//
// chrome_path: the root path of Chrome installation.
// latest_version_str: the latest version of Chrome installed.
void RemoveOldVersionDirs(const std::wstring& chrome_path,
                          const std::wstring& latest_version_str);

}  // namespace installer

#endif
