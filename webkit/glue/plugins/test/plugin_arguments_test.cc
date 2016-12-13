// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/string_util.h"

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
        std::string arg_name = StringPrintf("%s%d", "val", index);
        const char *val_string = GetArgValue(arg_name.c_str(), argc, argn,
                                             argv);
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
