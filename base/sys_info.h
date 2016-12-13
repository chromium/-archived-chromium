// Copyright (c) 2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_SYS_INFO_H_
#define BASE_SYS_INFO_H_

#include "base/basictypes.h"

#include <string>

namespace base {

class SysInfo {
 public:
  // Return the number of logical processors/cores on the current machine.
  static int NumberOfProcessors();

  // Return the number of bytes of physical memory on the current machine.
  static int64 AmountOfPhysicalMemory();

  // Return the number of megabytes of physical memory on the current machine.
  static int AmountOfPhysicalMemoryMB() {
    return static_cast<int>(AmountOfPhysicalMemory() / 1024 / 1024);
  }

  // Return the available disk space in bytes on the volume containing |path|,
  // or -1 on failure.
  static int64 AmountOfFreeDiskSpace(const std::wstring& path);

  // Return true if the given environment variable is defined.
  // TODO: find a better place for HasEnvVar.
  static bool HasEnvVar(const wchar_t* var);

  // Return the value of the given environment variable
  // or an empty string if not defined.
  // TODO: find a better place for GetEnvVar.
  static std::wstring GetEnvVar(const wchar_t* var);

  // Returns the name of the host operating system.
  static std::string OperatingSystemName();

  // Returns the version of the host operating system.
  static std::string OperatingSystemVersion();

  // Retrieves detailed numeric values for the OS version.
  // TODO(port): Implement a Linux version of this method and enable the
  // corresponding unit test.
  static void OperatingSystemVersionNumbers(int32 *major_version,
                                            int32 *minor_version,
                                            int32 *bugfix_version);

  // Returns the CPU architecture of the system. Exact return value may differ
  // across platforms.
  static std::string CPUArchitecture();

  // Returns the pixel dimensions of the primary display via the
  // width and height parameters.
  static void GetPrimaryDisplayDimensions(int* width, int* height);

  // Return the number of displays.
  static int DisplayCount();

  // Return the smallest amount of memory (in bytes) which the VM system will
  // allocate.
  static size_t VMAllocationGranularity();
};

}  // namespace base

#endif  // BASE_SYS_INFO_H_
