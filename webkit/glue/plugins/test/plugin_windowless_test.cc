// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#define STRSAFE_NO_DEPRECATE
#include "webkit/glue/plugins/test/plugin_windowless_test.h"
#include "webkit/glue/plugins/test/plugin_client.h"

namespace NPAPIClient {

// Remember the first plugin instance for tests involving multiple instances
WindowlessPluginTest* g_other_instance = NULL;

WindowlessPluginTest::WindowlessPluginTest(
    NPP id, NPNetscapeFuncs *host_functions, const std::string& test_name)
  : PluginTest(id, host_functions),
    test_name_(test_name) {
  if (!g_other_instance)
    g_other_instance = this;
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
  if (WM_PAINT == np_event->event) {
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

    if (test_name_ == "execute_script_delete_in_paint") {
      ExecuteScriptDeleteInPaint(browser);
    } else if (test_name_ == "multiple_instances_sync_calls") {
      MultipleInstanceSyncCalls(browser);
    }

  } else if (WM_MOUSEMOVE == np_event->event &&
             test_name_ == "execute_script_delete_in_mouse_move") {
    ExecuteScript(browser, id(), "DeletePluginWithinScript();", NULL);
    SignalTestCompleted();
  } else if (WM_LBUTTONUP == np_event->event &&
            test_name_ == "delete_frame_test") {
    ExecuteScript(
        browser, id(),
        "parent.document.getElementById('frame').outerHTML = ''", NULL);
  }
  // If this test failed, then we'd have crashed by now.
  return PluginTest::HandleEvent(event);
}

NPError WindowlessPluginTest::ExecuteScript(NPNetscapeFuncs* browser, NPP id,
    const std::string& script, NPVariant* result) {
  std::string script_url = "javascript:";
  script_url += script;

  NPString script_string = { script_url.c_str(), script_url.length() };
  NPObject *window_obj = NULL;
  browser->getvalue(id, NPNVWindowNPObject, &window_obj);

  NPVariant unused_result;
  if (!result)
    result = &unused_result;

  return browser->evaluate(id, window_obj, &script_string, result);
}

void WindowlessPluginTest::ExecuteScriptDeleteInPaint(
    NPNetscapeFuncs* browser) {
  NPUTF8* urlString = "javascript:DeletePluginWithinScript()";
  NPUTF8* targetString = NULL;
  browser->geturl(id(), urlString, targetString);
  SignalTestCompleted();
}

void WindowlessPluginTest::MultipleInstanceSyncCalls(NPNetscapeFuncs* browser) {
  if (this == g_other_instance)
    return;

  DCHECK(g_other_instance);
  ExecuteScript(browser, g_other_instance->id(), "TestCallback();", NULL);
  SignalTestCompleted();
}

} // namespace NPAPIClient
