// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/accessibility/registry_util.h"

#include <ocidl.h>

#include "chrome/test/accessibility/constants.h"

BSTR GetChromeExePath() {
  BSTR chrome_exe_path = SysAllocString(CHROME_PATH);
  return chrome_exe_path;
}
