// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "registry_util.h"
#include "constants.h"

BSTR GetChromeExePath() {
  // TODO: once registry contains chrome exe path.
  BSTR chrome_exe_path = SysAllocString(CHROME_PATH);
  return chrome_exe_path;
}

