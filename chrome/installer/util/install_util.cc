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
// See the corresponding header file for description of the functions in this
// file.

#include "install_util.h"

#include <windows.h>

#include "base/logging.h"
#include "base/registry.h"
#include "base/string_util.h"
#include "chrome/installer/util/google_update_constants.h"


std::wstring InstallUtil::GetChromeGoogleUpdateKey() {
  std::wstring chrome_google_update_key(google_update::kRegPathClients);
  chrome_google_update_key.append(L"\\");
  chrome_google_update_key.append(google_update::kChromeGuid);
  return chrome_google_update_key;
}

installer::Version* InstallUtil::GetChromeVersion(bool system_install) {
  RegKey key;
  std::wstring version_str;

  HKEY reg_root = (system_install) ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;
  if (!key.Open(reg_root, GetChromeGoogleUpdateKey().c_str(), KEY_READ) ||
      !key.ReadValue(google_update::kRegVersionField, &version_str)) {
    LOG(INFO) << "No existing Chrome install found.";
    key.Close();
    return NULL;
  }
  key.Close();
  LOG(INFO) << "Existing Chrome version found " << version_str;
  return installer::Version::GetVersionFromString(version_str);
}

std::wstring InstallUtil::GetNewGoogleUpdateApKey(bool diff_install,
    installer_util::InstallStatus status, const std::wstring& value) {
  // Magic suffix that we need to add or remove to "ap" key value.
  const std::wstring kMagicSuffix = L"-full";

  bool has_magic_string = false;
  if ((value.length() >= kMagicSuffix.length()) &&
      (value.rfind(kMagicSuffix) == (value.length() - kMagicSuffix.length()))) {
    LOG(INFO) << "Incremental installer failure key already set.";
    has_magic_string = true;
  }

  std::wstring new_value(value);
  if ((!diff_install || InstallSuccessful(status)) && has_magic_string) {
    LOG(INFO) << "Removing failure key from value " << value;
    new_value = value.substr(0, value.length() - kMagicSuffix.length());
  } else if ((diff_install && !InstallSuccessful(status)) &&
             !has_magic_string) {
    LOG(INFO) << "Incremental installer failed, setting failure key.";
    new_value.append(kMagicSuffix);
  }

  return new_value;
}

bool InstallUtil::InstallSuccessful(installer_util::InstallStatus status) {
  switch (status) {
    case installer_util::FIRST_INSTALL_SUCCESS:
    case installer_util::INSTALL_REPAIRED:
    case installer_util::NEW_VERSION_UPDATED:
    case installer_util::HIGHER_VERSION_EXISTS:
      return true;
    default:
      return false;
  }
}
