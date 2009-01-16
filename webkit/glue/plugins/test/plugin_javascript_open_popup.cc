// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "webkit/glue/plugins/test/plugin_javascript_open_popup.h"

#include "webkit/glue/plugins/test/plugin_client.h"

namespace NPAPIClient {

ExecuteJavascriptOpenPopupWithPluginTest::
    ExecuteJavascriptOpenPopupWithPluginTest(NPP id,
                                             NPNetscapeFuncs *host_functions)
    : PluginTest(id, host_functions),
      popup_window_test_started_(false) {
}

int16 ExecuteJavascriptOpenPopupWithPluginTest::SetWindow(
    NPWindow* window) {
  if (!popup_window_test_started_) {
    popup_window_test_started_ = true;
    HostFunctions()->geturl(
        id(), "popup_window_with_target_plugin.html", "_blank");
  }
  return PluginTest::SetWindow(window);
}

// ExecuteJavascriptPopupWindowTargetPluginTest member defines.
ExecuteJavascriptPopupWindowTargetPluginTest::
    ExecuteJavascriptPopupWindowTargetPluginTest(
        NPP id, NPNetscapeFuncs* host_functions)
    : PluginTest(id, host_functions),
      test_completed_(false) {
}

int16 ExecuteJavascriptPopupWindowTargetPluginTest::SetWindow(
    NPWindow* window) {
  if (!test_completed_) {
    HWND window_handle = reinterpret_cast<HWND>(window->window);

    if (IsWindow(window_handle)) {
      HWND parent_window = GetParent(window_handle);
      if (!IsWindow(parent_window) || parent_window == GetDesktopWindow()) {
        SetError("Windowed plugin instantiated with NULL parent");
      }
      SignalTestCompleted();
      test_completed_ = true;
    }
  }
  return PluginTest::SetWindow(window);
}

} // namespace NPAPIClient
