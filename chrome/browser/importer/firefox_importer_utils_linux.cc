// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/importer/firefox_importer_utils.h"

FilePath GetProfilesINI() {
  FilePath ini_file;
  // The default location of the profile folder containing user data is
  // under user HOME directory in .mozilla/firefox folder on Linux.
  const char *home = getenv("HOME");
  if (home && home[0]) {
    ini_file = FilePath(home).Append(".mozilla/firefox/profiles.ini");
  }
  if (file_util::PathExists(ini_file))
    return ini_file;

  return FilePath();
}

// static
const wchar_t NSSDecryptor::kNSS3Library[] = L"libnss3.so";
const wchar_t NSSDecryptor::kSoftokn3Library[] = L"libsoftokn3.so";
const wchar_t NSSDecryptor::kPLDS4Library[] = L"libplds4.so";
const wchar_t NSSDecryptor::kNSPR4Library[] = L"libnspr4.so";

bool NSSDecryptor::Init(const std::wstring& dll_path,
                        const std::wstring& db_path) {
  nss3_dll_ = base::LoadNativeLibrary(
      FilePath::FromWStringHack(kNSS3Library));
  if (nss3_dll_ == NULL)
    return false;
  base::NativeLibrary plds4_lib = base::LoadNativeLibrary(
      FilePath::FromWStringHack(kPLDS4Library));
  base::NativeLibrary nspr4_lib = base::LoadNativeLibrary(
      FilePath::FromWStringHack(kNSPR4Library));

  return InitNSS(db_path, plds4_lib, nspr4_lib);
}
