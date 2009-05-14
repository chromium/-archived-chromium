// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_PLUGINS_TEST_PLUGIN_WINDOWED_TEST_H
#define WEBKIT_GLUE_PLUGINS_TEST_PLUGIN_WINDOWED_TEST_H

#include "webkit/glue/plugins/test/plugin_test.h"

namespace NPAPIClient {

// This class contains a list of windowed plugin tests. Please add additional
// tests to this class.
class WindowedPluginTest : public PluginTest {
 public:
  WindowedPluginTest(NPP id, NPNetscapeFuncs *host_functions);
  ~WindowedPluginTest();
  virtual NPError SetWindow(NPWindow* pNPWindow);

 private:
  static LRESULT CALLBACK WindowProc(
      HWND window, UINT message, WPARAM wparam, LPARAM lparam);
  static void CallJSFunction(WindowedPluginTest*, const char*);

  HWND window_;
  bool done_;
};

} // namespace NPAPIClient

#endif  // WEBKIT_GLUE_PLUGINS_TEST_PLUGIN_WINDOWED_TEST_H
