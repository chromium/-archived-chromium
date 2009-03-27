// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#define STRSAFE_NO_DEPRECATE
#include "webkit/glue/plugins/test/plugin_windowless_test.h"
#include "webkit/glue/plugins/test/plugin_client.h"

namespace NPAPIClient {

WindowlessPluginTest::WindowlessPluginTest(
    NPP id, NPNetscapeFuncs *host_functions, const std::string& test_name)
  : PluginTest(id, host_functions),
    test_name_(test_name) {
}

int16 WindowlessPluginTest::HandleEvent(void* event) {

  NPNetscapeFuncs* browser = NPAPIClient::PluginClient::HostFunctions();

  NPBool supports_windowless = 0;
  NPError result = browser->getvalue(id(), NPNVSupportsWindowless,
                                     &supports_windowless);
  if ((result != NPERR_NO_ERROR) || (supports_windowless != TRUE)) {
    SetError("Failed to read NPNVSupportsWindowless value");
    SignalTestCompleted();
    return PluginTest::HandleEvent(event);
  }

  NPEvent* np_event = reinterpret_cast<NPEvent*>(event);
  if (WM_PAINT == np_event->event &&
      base::strcasecmp(test_name_.c_str(),
                       "execute_script_delete_in_paint") == 0) {
    HDC paint_dc = reinterpret_cast<HDC>(np_event->wParam);
    if (paint_dc == NULL) {
      SetError("Invalid Window DC passed to HandleEvent for WM_PAINT");
      SignalTestCompleted();
      return NPERR_GENERIC_ERROR;
    }

    HRGN clipping_region = CreateRectRgn(0, 0, 0, 0);
    if (!GetClipRgn(paint_dc, clipping_region)) {
      SetError("No clipping region set in window DC");
      DeleteObject(clipping_region);
      SignalTestCompleted();
      return NPERR_GENERIC_ERROR;
    }

    DeleteObject(clipping_region);

    NPUTF8* urlString = "javascript:DeletePluginWithinScript()";
    NPUTF8* targetString = NULL;
    browser->geturl(id(), urlString, targetString);
    SignalTestCompleted();
  } else if (WM_MOUSEMOVE == np_event->event &&
             base::strcasecmp(test_name_.c_str(),
                              "execute_script_delete_in_mouse_move") == 0) {
    std::string script = "javascript:DeletePluginWithinScript()";
    NPString script_string;
    script_string.UTF8Characters = script.c_str();
    script_string.UTF8Length =
        static_cast<unsigned int>(script.length());

    NPObject *window_obj = NULL;
    browser->getvalue(id(), NPNVWindowNPObject, &window_obj);
    NPVariant result_var;
    NPError result = browser->evaluate(id(), window_obj,
                                       &script_string, &result_var);
    SignalTestCompleted();
  }
  // If this test failed, then we'd have crashed by now.
  return PluginTest::HandleEvent(event);
}

} // namespace NPAPIClient
