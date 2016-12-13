// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/glue/plugins/test/plugin_windowed_test.h"
#include "webkit/glue/plugins/test/plugin_client.h"

namespace NPAPIClient {

WindowedPluginTest::WindowedPluginTest(NPP id, NPNetscapeFuncs *host_functions)
    : PluginTest(id, host_functions),
      window_(NULL), done_(false) {
}

WindowedPluginTest::~WindowedPluginTest() {
  if (window_)
    DestroyWindow(window_);
}

NPError WindowedPluginTest::SetWindow(NPWindow* pNPWindow) {
  if (test_name() == "create_instance_in_paint" && test_id() == "2") {
    SignalTestCompleted();
    return NPERR_NO_ERROR;
  }

  if (window_)
    return NPERR_NO_ERROR;

  HWND parent = reinterpret_cast<HWND>(pNPWindow->window);
  if (!pNPWindow || !::IsWindow(parent)) {
    SetError("Invalid arguments passed in");
    return NPERR_INVALID_PARAM;
  }

  if ((test_name() == "create_instance_in_paint" && test_id() == "1") ||
      test_name() == "alert_in_window_message") {
    static ATOM window_class = 0;
    if (!window_class) {
      WNDCLASSEX wcex;
      wcex.cbSize         = sizeof(WNDCLASSEX);
      wcex.style          = CS_DBLCLKS;
      wcex.lpfnWndProc    = &NPAPIClient::WindowedPluginTest::WindowProc;
      wcex.cbClsExtra     = 0;
      wcex.cbWndExtra     = 0;
      wcex.hInstance      = GetModuleHandle(NULL);
      wcex.hIcon          = 0;
      wcex.hCursor        = 0;
      wcex.hbrBackground  = reinterpret_cast<HBRUSH>(COLOR_WINDOW+1);
      wcex.lpszMenuName   = 0;
      wcex.lpszClassName  = L"CreateInstanceInPaintTestWindowClass";
      wcex.hIconSm        = 0;
      window_class = RegisterClassEx(&wcex);
    }

    window_ = CreateWindowEx(
        WS_EX_LEFT | WS_EX_LTRREADING | WS_EX_RIGHTSCROLLBAR,
        MAKEINTATOM(window_class), 0,
        WS_CHILD | WS_CLIPCHILDREN | WS_CLIPSIBLINGS | WS_VISIBLE ,
        0, 0, 100, 100, parent, 0, GetModuleHandle(NULL), 0);
    DCHECK(window_);
    ::SetProp(window_, L"Plugin_Instance", this);
  }

  return NPERR_NO_ERROR;
}

void WindowedPluginTest::CallJSFunction(
    WindowedPluginTest* this_ptr, const char* function) {
  NPIdentifier function_id = this_ptr->HostFunctions()->getstringidentifier(
      function);

  NPObject *window_obj = NULL;
  this_ptr->HostFunctions()->getvalue(
      this_ptr->id(), NPNVWindowNPObject, &window_obj);

  NPVariant rv;
  this_ptr->HostFunctions()->invoke(
      this_ptr->id(), window_obj, function_id, NULL, 0, &rv);
}

LRESULT CALLBACK WindowedPluginTest::WindowProc(
    HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
  WindowedPluginTest* this_ptr =
      reinterpret_cast<WindowedPluginTest*>
          (::GetProp(window, L"Plugin_Instance"));

  if (this_ptr && !this_ptr->done_) {
    if (this_ptr->test_name() == "create_instance_in_paint" &&
        message == WM_PAINT) {
      this_ptr->done_ = true;
      CallJSFunction(this_ptr, "CreateNewInstance");
    } else if (this_ptr->test_name() == "alert_in_window_message" &&
               message == WM_PAINT) {
      this_ptr->done_ = true;
      // We call this function twice as we want to display two alerts
      // and verify that we don't hang the browser.
      CallJSFunction(this_ptr, "CallAlert");
      CallJSFunction(this_ptr, "CallAlert");
    }
  }

  return DefWindowProc(window, message, wparam, lparam);
}

} // namespace NPAPIClient
