// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include <wtf/MainThread.h>
#include <wtf/Threading.h>
#undef LOG

#include "build/build_config.h"

#include "webkit/tools/test_shell/test_webworker_helper.h"

#include "base/logging.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "third_party/WebKit/WebKit/chromium/public/WebKit.h"
#include "webkit/glue/webworkerclient.h"


WebWorker* TestWebWorkerHelper::CreateWebWorker(WebWorkerClient* client) {
  TestWebWorkerHelper* loader = new TestWebWorkerHelper();
  return loader->CreateWebWorker_(client, loader);
}

TestWebWorkerHelper::TestWebWorkerHelper() :
#if defined(OS_WIN)
      module_(NULL),
#endif
      CreateWebWorker_(NULL) {
  Load();
}

TestWebWorkerHelper::~TestWebWorkerHelper() {
}

bool TestWebWorkerHelper::IsMainThread() const {
  return WTF::isMainThread();
}

void TestWebWorkerHelper::DispatchToMainThread(WTF::MainThreadFunction* func,
                                               void* context) {
  return WTF::callOnMainThread(func, context);
}

bool TestWebWorkerHelper::Load() {
#if defined(OS_WIN)
  FilePath path;
  PathService::Get(base::DIR_EXE, &path);
  path = path.AppendASCII("test_worker.dll");

  module_ = LoadLibrary(path.value().c_str());
  if (module_ == 0)
    return false;

  CreateWebWorker_ = reinterpret_cast<CreateWebWorkerFunc>
      (GetProcAddress(module_, "CreateWebWorker"));
  if (!CreateWebWorker_) {
    FreeLibrary(module_);
    module_ = 0;
    return false;
  }

  return true;
#else
  NOTIMPLEMENTED();
  return false;
#endif
}

void TestWebWorkerHelper::Unload() {
  // Since this is called from DLL, delay the unloading until it can be
  // invoked from EXE.
  return WTF::callOnMainThread(UnloadHelper, this);
}

void TestWebWorkerHelper::UnloadHelper(void* param) {
  TestWebWorkerHelper* this_ptr = static_cast<TestWebWorkerHelper*>(param);

#if defined(OS_WIN)
  if (this_ptr->module_) {
    FreeLibrary(this_ptr->module_);
    this_ptr->module_ = 0;
  }
#else
  NOTIMPLEMENTED();
#endif

  delete this_ptr;
}
