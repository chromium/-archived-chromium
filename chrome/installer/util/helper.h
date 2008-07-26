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

// Launches Chrome without waiting for its exit.
bool LaunchChrome(bool system_install);

// Launches Chrome with given command line, waits for Chrome to terminate
// within timeout_ms, and gets the process exit code if available.
// The function returns true as long as Chrome is successfully launched.
// The status of Chrome at the return of the function is given by exit_code
// and is_timeout.
bool LaunchChromeAndWaitForResult(bool system_install,
                                  const std::wstring& options,
                                  int32 timeout_ms,
                                  int32* exit_code,
                                  bool* is_timeout);

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
