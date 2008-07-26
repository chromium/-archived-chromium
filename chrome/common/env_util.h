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

// This file defines utility functions that can report details about the
// host operating environment.

#ifndef CHROME_COMMON_ENV_UTIL_H__
#define CHROME_COMMON_ENV_UTIL_H__

#include <windows.h>
#include <string>

namespace env_util {

// Test if the given environment variable is defined.
inline bool HasEnvironmentVariable(const wchar_t* var) {
  return GetEnvironmentVariable(var, NULL, 0) != 0;
}

// Returns the name of the host operating system.
std::string GetOperatingSystemName();

// Returns the version of the host operating system.
std::string GetOperatingSystemVersion();

// Returns the total amount of physical memory present.
int GetPhysicalMemoryMB();

// Returns the CPU architecture of the system.
std::string GetCPUArchitecture();

// Returns the pixel dimensions of the primary display via the
// width and height parameters.
void GetPrimaryDisplayDimensions(int* width, int* height);

// Return the number of displays.
int GetDisplayCount();

}  // namespace env_util

#endif  // CHROME_COMMON_ENV_UTIL_H__
