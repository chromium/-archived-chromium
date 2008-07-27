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

//
// npapitest
//
// This is a NPAPI Plugin Program which is used to test the Browser's NPAPI
// host implementation.  It is used in conjunction with the npapi_unittest.
//
// As a NPAPI Plugin, you can invoke it by creating a web page of the following
// type:
//
//     <embed src="content-to-load" type="application/vnd.npapi-test"
//      name="test-name">
//
// arguments:
//     src:      This is the initial content which will be sent to the plugin.
//     type:     Must be "application/vnd.npapi-test"
//     name:     The testcase to run when invoked
//     id:       The id of the test being run (for testing concurrent plugins)
//
// The Plugin drives the actual test, calling host functions and validating the
// Host callbacks which it receives.  It is the duty of the plugin to record
// all errors.
//
// To indicate test completion, the plugin expects the containing HTML page to
// implement two javascript functions:
//     onSuccess(string testname);
//     onFailure(string testname, string results);
// The HTML host pages used in this test will then set a document cookie
// which the automated test framework can poll for and discover that the
// test has completed.
//
//
// TESTS
// When the PluginClient receives a NPP_New callback from the browser,
// it looks at the "name" argument which is passed in.  It verifies that
// the name matches a known test, and instantiates that test.  The test is
// a subclass of PluginTest.
//
//

#include <windows.h>
#include "webkit/glue/plugins/test/plugin_client.h"

BOOL WINAPI DllMain(HINSTANCE hDll, DWORD dwReason, LPVOID lpReserved) {
  return TRUE;
}

extern "C" {
NPError WINAPI NP_GetEntryPoints(NPPluginFuncs* pFuncs) {
  return NPAPIClient::PluginClient::GetEntryPoints(pFuncs);
}

NPError WINAPI NP_Initialize(NPNetscapeFuncs* pFuncs) {
  return NPAPIClient::PluginClient::Initialize(pFuncs);
}

NPError WINAPI NP_Shutdown() {
  return NPAPIClient::PluginClient::Shutdown();
}
} // extern "C"

namespace WebCore {
  const char* currentTextBreakLocaleID() { return "en_us"; }
}