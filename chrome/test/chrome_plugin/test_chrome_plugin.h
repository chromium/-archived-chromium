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
// Shared by the plugin DLL and the unittest code.

#ifndef CHROME_TEST_CHROME_PLUGIN_TEST_CHROME_PLUGIN_H__
#define CHROME_TEST_CHROME_PLUGIN_TEST_CHROME_PLUGIN_H__

#include <string>

#include "base/basictypes.h"
#include "chrome/common/chrome_plugin_api.h"

class GURL;

struct TestResponsePayload {
  const char* url;
  bool async;
  int status;
  const char* mime_type;
  const char* body;
};

const char kChromeTestPluginProtocol[] = "cptest";

const TestResponsePayload kChromeTestPluginPayloads[] = {
  {
    "cptest:sync",
    false,
    200,
    "text/html",
    "<head><title>cptest:sync</title></head><body>SUCCESS</body>"
  },
  {
    "cptest:async",
    true,
    200,
    "text/plain",
    "<head><title>cptest:async</title></head><body>SUCCESS</body>"
  },
  {
    "cptest:blank",
    false,
    200,
    "text/plain",
    ""
  },
};

struct TestFuncParams {
  typedef void (STDCALL *CallbackFunc)(void* data);

  struct PluginFuncs {
    int (STDCALL *test_make_request)(const char* method, const GURL& url);
  };
  PluginFuncs pfuncs;

  struct BrowserFuncs {
    void (STDCALL *test_complete)(CPRequest* request, bool success,
                                  const std::string& raw_headers,
                                  const std::string& body);
    void (STDCALL *invoke_later)(CallbackFunc callback, void* callback_data,
                                 int delay_ms);
  };
  BrowserFuncs bfuncs;
};

const char kChromeTestPluginPostData[] = "Test Data";

#endif  // CHROME_TEST_CHROME_PLUGIN_TEST_CHROME_PLUGIN_H__
