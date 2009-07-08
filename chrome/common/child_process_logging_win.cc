// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/child_process_logging.h"

#include <windows.h>

#include "base/string_util.h"
#include "chrome/common/chrome_constants.h"
#include "googleurl/src/gurl.h"

namespace child_process_logging {

typedef void (__cdecl *MainSetActiveURL)(const wchar_t*);

void SetActiveURL(const GURL& url) {
  HMODULE exe_module = GetModuleHandle(chrome::kBrowserProcessExecutableName);
  if (!exe_module)
    return;

  MainSetActiveURL set_active_url =
      reinterpret_cast<MainSetActiveURL>(
          GetProcAddress(exe_module, "SetActiveURL"));
  if (!set_active_url)
    return;

  (set_active_url)(UTF8ToWide(url.possibly_invalid_spec()).c_str());
}

}  // namespace child_process_logging
