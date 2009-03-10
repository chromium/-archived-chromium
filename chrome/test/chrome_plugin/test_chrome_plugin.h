// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
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
