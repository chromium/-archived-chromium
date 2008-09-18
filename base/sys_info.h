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

  // Return the available disk space in bytes on the volume containing |path|.
  static int64 AmountOfFreeDiskSpace(const std::wstring& path);

};

}  // namespace base

#endif  // BASE_SYS_INFO_H_
