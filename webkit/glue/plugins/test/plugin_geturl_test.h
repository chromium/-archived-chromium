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

#ifndef WEBKIT_PORT_PLUGINS_TEST_PLUGIN_GETURL_TEST_H__
#define WEBKIT_PORT_PLUGINS_TEST_PLUGIN_GETURL_TEST_H__

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
  HANDLE test_file_handle_;
};

} // namespace NPAPIClient

#endif  // WEBKIT_PORT_PLUGINS_TEST_PLUGIN_GETURL_TEST_H__

