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

#define STRSAFE_NO_DEPRECATE
#include <strsafe.h>
#include "webkit/glue/plugins/test/plugin_arguments_test.h"

namespace NPAPIClient {

PluginArgumentsTest::PluginArgumentsTest(NPP id,
                                         NPNetscapeFuncs *host_functions)
  : PluginTest(id, host_functions) {
}

NPError PluginArgumentsTest::New(uint16 mode, int16 argc,
                                 const char* argn[], const char* argv[],
                                 NPSavedData* saved) {
  // mode:  should be the string either "NP_EMBED" or "NP_FULL",
  //        depending on the mode passed in.
  // count: the count of "val" arguments.  If the value is
  //        2, then we'll find arguments "val1" and "val2".  If
  //        the value is 0, then there will be no "val" arguments.
  // size:  each val string will be this size * the value's
  //        index.  E.g if size is "10", val1 will be 10bytes,
  //        and val2 will be 20bytes.
  const char *mode_string = GetArgValue("mode", argc, argn, argv);
  ExpectAsciiStringNotEqual(mode_string, (const char *)NULL);
  if (mode_string != NULL) {
    std::string mode_dep_string = mode_string;
    if (mode == NP_EMBED)
      ExpectStringLowerCaseEqual(mode_dep_string, "np_embed");
    else if (mode == NP_FULL)
      ExpectStringLowerCaseEqual(mode_dep_string, "np_full");
  }

  const char *count_string = GetArgValue("count", argc, argn, argv);
  if (count_string != NULL) {
    int max_args = atoi(count_string);

    const char *size_string = GetArgValue("size", argc, argn, argv);
    ExpectAsciiStringNotEqual(size_string, (const char *)NULL);
    if (size_string != NULL) {
      int size = atoi(size_string);

      for (int index = 1; index <= max_args; index++) {
        char arg_name[MAX_PATH];  // Use MAX_PATH for Max Name Length
        StringCchPrintfA(arg_name, sizeof(arg_name), "%s%d", "val", index);
        const char *val_string = GetArgValue(arg_name, argc, argn, argv);
        ExpectAsciiStringNotEqual(val_string, (const char*)NULL);
        if (val_string != NULL)
          ExpectIntegerEqual((int)strlen(val_string), (index*size));
      }
    }
  }

  return PluginTest::New(mode, argc, argn, argv, saved);
}

NPError PluginArgumentsTest::SetWindow(NPWindow* pNPWindow) {
  // This test just tests the arguments.  We're done now.
  this->SignalTestCompleted();

  return NPERR_NO_ERROR;
}

} // namespace NPAPIClient
