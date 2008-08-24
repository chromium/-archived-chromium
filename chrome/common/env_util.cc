// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/env_util.h"

#include "base/basictypes.h"
#include "base/logging.h"

static const DWORDLONG kBytesPerMegabyte = 1048576;

namespace env_util {

std::string GetOperatingSystemName() {
  return "Windows";
}

std::string GetOperatingSystemVersion() {
  OSVERSIONINFO info = {0};
  info.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
  GetVersionEx(&info);

  char result[20];
  memset(result, 0, arraysize(result));
  _snprintf_s(result, arraysize(result),
               "%lu.%lu", info.dwMajorVersion, info.dwMinorVersion);
  return std::string(result);
}

int GetPhysicalMemoryMB() {
  MEMORYSTATUSEX status;
  status.dwLength = sizeof(status);
  if (::GlobalMemoryStatusEx(&status))
    return static_cast<int>(status.ullTotalPhys / kBytesPerMegabyte);

  NOTREACHED() << "GlobalMemoryStatusEx failed.";
  return 0;
}

std::string GetCPUArchitecture() {
  // TODO: Make this vary when we support any other architectures.
  return "x86";
}

void GetPrimaryDisplayDimensions(int* width, int* height) {
  if (width)
    *width = GetSystemMetrics(SM_CXSCREEN);

  if (height)
    *height = GetSystemMetrics(SM_CYSCREEN);
}

int GetDisplayCount() {
  return GetSystemMetrics(SM_CMONITORS);
}

}  // namespace env_util

