// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/sys_info.h"

#include <CoreServices/CoreServices.h>

namespace base {

// static
void SysInfo::OperatingSystemVersionNumbers(int32 *major_version,
                                            int32 *minor_version,
                                            int32 *bugfix_version) {
  int32 major_version_cached = 0;
  int32 minor_version_cached = 0;
  int32 bugfix_version_cached = 0;

  // Gestalt can't be called in the sandbox, so we cache its return value.
  Gestalt(gestaltSystemVersionMajor,
      reinterpret_cast<SInt32*>(&major_version_cached));
  Gestalt(gestaltSystemVersionMinor,
      reinterpret_cast<SInt32*>(&minor_version_cached));
  Gestalt(gestaltSystemVersionBugFix,
      reinterpret_cast<SInt32*>(&bugfix_version_cached));

  *major_version = major_version_cached;
  *minor_version = minor_version_cached;
  *bugfix_version = bugfix_version_cached;
}

}  // namespace base
