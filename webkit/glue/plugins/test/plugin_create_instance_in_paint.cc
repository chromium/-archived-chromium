// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/glue/plugins/test/plugin_create_instance_in_paint.h"

#include "webkit/glue/plugins/test/plugin_client.h"

namespace NPAPIClient {

CreateInstanceInPaintTest::CreateInstanceInPaintTest(
    NPP id, NPNetscapeFuncs *host_functions)
    : PluginTest(id, host_functions),
      window_(NULL), created_(false) {
}

NPError CreateInstanceInPaintTest::SetWindow(NPWindow* pNPWindow) {
  if (test_id() == "1") {
    if (!window_) {
      static ATOM window_class = 0;
      if (!window_class) {
        WNDCLASSEX wcex;
        wcex.cbSize         = sizeof(WNDCLASSEX);
        wcex.style          = CS_DBLCLKS;
        wcex.lpfnWndProc    =
            &NPAPIClient::CreateInstanceInPaintTest::WindowProc;
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

      HWND parent = reinterpret_cast<HWND>(pNPWindow->window);
      window_ = CreateWindowEx(
          WS_EX_LEFT | WS_EX_LTRREADING | WS_EX_RIGHTSCROLLBAR,
          MAKEINTATOM(window_class), 0,
          WS_CHILD | WS_CLIPCHILDREN | WS_CLIPSIBLINGS | WS_VISIBLE ,
          0, 0, 100, 100, parent, 0, GetModuleHandle(NULL), 0);
      DCHECK(window_);
      ::SetProp(window_, L"Plugin_Instance", this);
    }
  } else if (test_id() == "2") {
    SignalTestCompleted();
  } else {
    NOTREACHED();
  }
  return NPERR_NO_ERROR;
}

LRESULT CALLBACK CreateInstanceInPaintTest::WindowProc(
    HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
  if (message == WM_PAINT) {
    CreateInstanceInPaintTest* this_instance =
        reinterpret_cast<CreateInstanceInPaintTest*>
            (::GetProp(window, L"Plugin_Instance"));
    if (this_instance->test_id() == "1" && !this_instance->created_) {
      this_instance->created_ = true;
      this_instance->HostFunctions()->geturlnotify(
          this_instance->id(), "javascript:CreateNewInstance()", NULL,
          reinterpret_cast<void*>(1));
    }
  }

  return DefWindowProc(window, message, wparam, lparam);
}

} // namespace NPAPIClient
