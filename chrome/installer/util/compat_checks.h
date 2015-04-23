// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_INSTALLER_UTIL_COMPAT_CHECKS_H_
#define CHROME_INSTALLER_UTIL_COMPAT_CHECKS_H_

// Returns true if this computer has a Symantec End Point version that
// is known to cause trouble. Non- null parameters are only used in testing.
bool HasIncompatibleSymantecEndpointVersion(const wchar_t* version);

#endif  // CHROME_INSTALLER_UTIL_COMPAT_CHECKS_H_
