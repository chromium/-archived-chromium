// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/glue/plugins/test/plugin_new_fails_test.h"

namespace NPAPIClient {

NewFailsTest::NewFailsTest(NPP id, NPNetscapeFuncs *host_functions)
    : PluginTest(id, host_functions) {
}

NPError NewFailsTest::New(uint16 mode, int16 argc, const char* argn[],
                             const char* argv[], NPSavedData* saved) {
  return NPERR_GENERIC_ERROR;
}

} // namespace NPAPIClient
