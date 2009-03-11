// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_PLUGINS_TEST_PLUGIN_WINDOW_SIZE_TEST_H
#define WEBKIT_GLUE_PLUGINS_TEST_PLUGIN_WINDOW_SIZE_TEST_H

#include "webkit/glue/plugins/test/plugin_test.h"

namespace NPAPIClient {

// This class tests whether the plugin window has a non zero rect
// on the second SetWindow call.
class PluginWindowSizeTest : public PluginTest {
 public:
  // Constructor.
  PluginWindowSizeTest(NPP id, NPNetscapeFuncs *host_functions);
  // NPAPI SetWindow handler
  virtual NPError SetWindow(NPWindow* pNPWindow);
};

} // namespace NPAPIClient

#endif  // WEBKIT_GLUE_PLUGINS_TEST_PLUGIN_WINDOW_SIZE_TEST_H
