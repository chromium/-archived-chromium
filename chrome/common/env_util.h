// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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

