// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/glue/plugins/test/plugin_windowed_test.h"
#include "webkit/glue/plugins/test/plugin_client.h"

namespace NPAPIClient {

WindowedPluginTest::WindowedPluginTest(NPP id, NPNetscapeFuncs *host_functions)
    : PluginTest(id, host_functions) {
}

NPError WindowedPluginTest::SetWindow(NPWindow* pNPWindow) {
  HWND window = reinterpret_cast<HWND>(pNPWindow->window);
  if (!pNPWindow || !::IsWindow(window)) {
    SetError("Invalid arguments passed in");
    return NPERR_INVALID_PARAM;
  }

  return NPERR_NO_ERROR;
}

} // namespace NPAPIClient
