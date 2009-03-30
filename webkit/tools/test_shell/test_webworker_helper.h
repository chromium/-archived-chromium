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

#include <wtf/MainThread.h>

class TestWebWorkerHelper;
class WebWorker;
class WebWorkerClient;

// Function to call in test_worker DLL.
typedef WebWorker* (API_CALL *CreateWebWorkerFunc)(
    WebWorkerClient* webworker_client,
    TestWebWorkerHelper* webworker_helper);;

class TestWebWorkerHelper {
 public:
  static WebWorker* CreateWebWorker(WebWorkerClient* client);

  TestWebWorkerHelper();
  ~TestWebWorkerHelper();

  virtual bool IsMainThread() const;
  virtual void DispatchToMainThread(WTF::MainThreadFunction* func,
                                    void* context);
  virtual void Unload();

 private:
  bool Load();
  static void UnloadHelper(void* param);

#if defined(OS_WIN)
  // TODO(port): Remove ifdefs when we have portable replacement for HMODULE.
  HMODULE module_;
#endif  // defined(OS_WIN)

  CreateWebWorkerFunc CreateWebWorker_;

  DISALLOW_COPY_AND_ASSIGN(TestWebWorkerHelper);
};

#endif  // WEBKIT_TOOLS_TEST_SHELL_TEST_WEBWORKER_HELPER_H__
