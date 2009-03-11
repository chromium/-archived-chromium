// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_PORT_PLUGINS_TEST_PLUGIN_ARGUMENTS_TEST_H__
#define WEBKIT_PORT_PLUGINS_TEST_PLUGIN_ARGUMENTS_TEST_H__

#include "webkit/glue/plugins/test/plugin_test.h"

namespace NPAPIClient {

// The PluginArgumentsTest test that we properly receive arguments
// intended for the plugin.
//
// This is basically overkill for testing that the arguments passed
// to the plugin match what we expect.
//
// We expect to find the following arguments:
// mode:  should be the string either "NP_EMBED" or "NP_FULL",
//        depending on the mode passed in.
// count: the count of "val" arguments.  If the value is
//        2, then we'll find arguments "val1" and "val2".  If
//        the value is 0, then there will be no "val" arguments.
// size:  each val string will be this size * the value's
//        index.  E.g if size is "10", val1 will be 10bytes,
//        and val2 will be 20bytes.
//
class PluginArgumentsTest : public PluginTest {
 public:
  // Constructor.
  PluginArgumentsTest(NPP id, NPNetscapeFuncs *host_functions);

  // Initialize this PluginTest based on the arguments from NPP_New.
  virtual NPError  New(uint16 mode, int16 argc, const char* argn[],
                       const char* argv[], NPSavedData* saved);

  // NPAPI SetWindow handler.
  virtual NPError SetWindow(NPWindow* pNPWindow);
};

} // namespace NPAPIClient

#endif  // WEBKIT_PORT_PLUGINS_TEST_PLUGIN_ARGUMENTS_TEST_H__
