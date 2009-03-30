// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/renderer_logging.h"

#include <windows.h>

#include "base/string_util.h"
#include "chrome/common/chrome_constants.h"
#include "googleurl/src/gurl.h"

namespace renderer_logging {

typedef void (__cdecl *MainSetActiveRendererURL)(const wchar_t*);

// Sets the URL that is logged if the renderer crashes. Use GURL() to clear
// the URL.
void SetActiveRendererURL(const GURL& url) {
  HMODULE exe_module = GetModuleHandle(chrome::kBrowserProcessExecutableName);
  if (!exe_module)
    return;

  MainSetActiveRendererURL set_active_renderer_url =
      reinterpret_cast<MainSetActiveRendererURL>(
          GetProcAddress(exe_module, "SetActiveRendererURL"));
  if (!set_active_renderer_url)
    return;

  (set_active_renderer_url)(UTF8ToWide(url.possibly_invalid_spec()).c_str());
}

}  // namespace renderer_logging
