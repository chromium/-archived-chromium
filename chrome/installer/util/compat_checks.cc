// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/util/compat_checks.h"

#include "base/registry.h"
#include "base/string_util.h"

namespace {

// SEP stands for Symantec End Point Protection.
std::wstring GetSEPVersion() {
  const wchar_t kProductKey[] =
      L"SOFTWARE\\Symantec\\Symantec Endpoint Protection\\SMC";
  RegKey key(HKEY_LOCAL_MACHINE, kProductKey, KEY_READ);
  std::wstring version_str;
  key.ReadValue(L"ProductVersion", &version_str);
  return version_str;
}

// The product version should be a string like "11.0.3001.2224". This function
// returns as params the first 3 values. Return value is false if anything
// does not fit the format.
bool ParseSEPVersion(const std::wstring& version, int* v0, int* v1, int* v2) {
  std::vector<std::wstring> v;
  SplitString(version, L'.', &v);
  if (v.size() != 4)
    return false;
  if (!StringToInt(v[0], v0))
    return false;
  if (!StringToInt(v[1], v1))
    return false;
  if (!StringToInt(v[2], v2))
    return false;
  return true;
}

// The incompatible versions are anything before 11MR3, which is 11.0.3001.
bool IsBadSEPVersion(int v0, int v1, int v2) {
  if (v0 < 11)
    return true;
  if (v1 > 0)
    return false;
  if (v2 < 3001)
    return true;
  return false;
}

}  // namespace

bool HasIncompatibleSymantecEndpointVersion(const wchar_t* version) {
  int v0, v1, v2;
  std::wstring ver_str(version ? version : GetSEPVersion());
  if (!ParseSEPVersion(ver_str, &v0, &v1, &v2))
    return false;
  return IsBadSEPVersion(v0, v1, v2);
}
