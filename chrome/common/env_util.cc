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
