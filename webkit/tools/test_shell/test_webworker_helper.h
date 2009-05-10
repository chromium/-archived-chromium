// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_TOOLS_TEST_SHELL_TEST_WEBWORKER_HELPER_H__
#define WEBKIT_TOOLS_TEST_SHELL_TEST_WEBWORKER_HELPER_H__

#include <vector>
#if defined(OS_WIN)
#include <windows.h>
#endif

#include "base/basictypes.h"
#include "base/port.h"
#include "webkit/api/public/WebString.h"

#include <wtf/MainThread.h>

class TestWebWorkerHelper;

namespace WebKit {
class WebKitClient;
class WebWorker;
class WebWorkerClient;
}

// Function to call in test_worker DLL.
typedef WebKit::WebWorker* (API_CALL *CreateWebWorkerFunc)(
    WebKit::WebWorkerClient* webworker_client,
    TestWebWorkerHelper* webworker_helper);;

class TestWebWorkerHelper {
 public:
  static WebKit::WebWorker* CreateWebWorker(WebKit::WebWorkerClient* client);

  TestWebWorkerHelper();

  virtual void DispatchToMainThread(void (*func)());
  virtual void Unload();

  virtual WebKit::WebString DuplicateString(const WebKit::WebString& string);

 private:
  ~TestWebWorkerHelper();
  void Load();
  static void UnloadHelper(void* param);

#if defined(OS_WIN)
  // TODO(port): Remove ifdefs when we have portable replacement for HMODULE.
  HMODULE module_;
#elif defined(OS_MACOSX)
  void* module_;
#endif

  CreateWebWorkerFunc CreateWebWorker_;
  int worker_count_;

  DISALLOW_COPY_AND_ASSIGN(TestWebWorkerHelper);
};

#endif  // WEBKIT_TOOLS_TEST_SHELL_TEST_WEBWORKER_HELPER_H__
