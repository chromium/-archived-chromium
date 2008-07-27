// Copyright 2008, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

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

