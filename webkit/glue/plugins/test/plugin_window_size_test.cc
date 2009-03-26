// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/glue/plugins/test/plugin_window_size_test.h"
#include "webkit/glue/plugins/test/plugin_client.h"

namespace NPAPIClient {

PluginWindowSizeTest::PluginWindowSizeTest(NPP id,
                                           NPNetscapeFuncs *host_functions)
    : PluginTest(id, host_functions) {
}

NPError PluginWindowSizeTest::SetWindow(NPWindow* pNPWindow) {
  HWND window = reinterpret_cast<HWND>(pNPWindow->window);
  if (!pNPWindow || !::IsWindow(window)) {
    SetError("Invalid arguments passed in");
    return NPERR_INVALID_PARAM;
  }

  RECT window_rect = {0};
  window_rect.left = pNPWindow->x;
  window_rect.top = pNPWindow->y;
  window_rect.right = pNPWindow->width;
  window_rect.bottom = pNPWindow->height;

  if (!::IsRectEmpty(&window_rect)) {
    RECT client_rect = {0};
    ::GetClientRect(window, &client_rect);
    if (::IsRectEmpty(&client_rect)) {
      SetError("The client rect of the plugin window is empty. Test failed");
    }

    // Bug 6742: ensure that the coordinates passed in are relative to the
    // parent HWND.
    POINT origin_from_os;
    RECT window_rect_from_os;
    ::GetWindowRect(window, &window_rect_from_os);
    origin_from_os.x = window_rect_from_os.left;
    origin_from_os.y = window_rect_from_os.top;
    ::ScreenToClient(GetParent(window), &origin_from_os);
    if (origin_from_os.x != pNPWindow->x || origin_from_os.y != pNPWindow->y)
      SetError("Wrong position passed in to SetWindow!  Test failed");

    SignalTestCompleted();
  }

  return NPERR_NO_ERROR;
}

} // namespace NPAPIClient
