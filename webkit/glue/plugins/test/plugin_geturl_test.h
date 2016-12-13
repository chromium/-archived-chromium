// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_PORT_PLUGINS_TEST_PLUGIN_GETURL_TEST_H__
#define WEBKIT_PORT_PLUGINS_TEST_PLUGIN_GETURL_TEST_H__

#include <stdio.h>

#include "webkit/glue/plugins/test/plugin_test.h"

namespace NPAPIClient {

// The PluginGetURLTest test functionality of the NPN_GetURL
// and NPN_GetURLNotify methods.
//
// This test first discovers it's URL by sending a GetURL request
// for 'javascript:top.location'.  After receiving that, the
// test will request the url itself (again via GetURL).
class PluginGetURLTest : public PluginTest {
 public:
  // Constructor.
  PluginGetURLTest(NPP id, NPNetscapeFuncs *host_functions);

  //
  // NPAPI functions
  //
  virtual NPError SetWindow(NPWindow* pNPWindow);
  virtual NPError NewStream(NPMIMEType type, NPStream* stream,
                            NPBool seekable, uint16* stype);
  virtual int32   WriteReady(NPStream *stream);
  virtual int32   Write(NPStream *stream, int32 offset, int32 len,
                        void *buffer);
  virtual NPError DestroyStream(NPStream *stream, NPError reason);
  virtual void    StreamAsFile(NPStream* stream, const char* fname);
  virtual void    URLNotify(const char* url, NPReason reason, void* data);

 private:
  bool tests_started_;
  int tests_in_progress_;
  std::string self_url_;
  FILE* test_file_;
};

} // namespace NPAPIClient

#endif  // WEBKIT_PORT_PLUGINS_TEST_PLUGIN_GETURL_TEST_H__
