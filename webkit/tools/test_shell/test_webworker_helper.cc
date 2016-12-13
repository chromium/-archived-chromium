// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include <wtf/MainThread.h>
#include <wtf/Threading.h>
#undef LOG

#include "build/build_config.h"

#include "webkit/tools/test_shell/test_webworker_helper.h"

#if defined(OS_MACOSX)
#include <dlfcn.h>
#endif

#include "base/logging.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "webkit/api/public/WebKit.h"
#include "webkit/api/public/WebKitClient.h"

using WebKit::WebWorker;
using WebKit::WebWorkerClient;

static TestWebWorkerHelper* g_helper;

WebWorker* TestWebWorkerHelper::CreateWebWorker(WebWorkerClient* client) {
  if (!g_helper)
    g_helper = new TestWebWorkerHelper();
  g_helper->worker_count_++;
  return g_helper->CreateWebWorker_(client, g_helper);
}

TestWebWorkerHelper::TestWebWorkerHelper() :
#if defined(OS_WIN)
      module_(NULL),
#endif
      CreateWebWorker_(NULL),
      worker_count_(0) {
  Load();
}

TestWebWorkerHelper::~TestWebWorkerHelper() {
}

void TestWebWorkerHelper::DispatchToMainThread(void (*func)()) {
  WebKit::webKitClient()->callOnMainThread(func);
}

void TestWebWorkerHelper::Load() {
  FilePath path;
  PathService::Get(base::DIR_EXE, &path);

#if defined(OS_WIN)
  path = path.AppendASCII("test_worker.dll");

  module_ = LoadLibrary(path.value().c_str());
  if (module_ == 0)
    return;

  CreateWebWorker_ = reinterpret_cast<CreateWebWorkerFunc>
      (GetProcAddress(module_, "CreateWebWorker"));
  if (!CreateWebWorker_) {
    FreeLibrary(module_);
    module_ = 0;
  }
#elif defined(OS_MACOSX)
  path = path.AppendASCII("test_worker.dylib");

  module_ = dlopen(path.value().c_str(), RTLD_NOW | RTLD_LOCAL);
  if (!module_)
    return;

  CreateWebWorker_ = reinterpret_cast<CreateWebWorkerFunc>
      (dlsym(module_, "CreateWebWorker"));
  if (!CreateWebWorker_) {
    dlclose(module_);
    module_ = 0;
  }
#else
  NOTIMPLEMENTED();
#endif
}

void TestWebWorkerHelper::Unload() {
  DCHECK(worker_count_);
  worker_count_--;
  // Since this is called from DLL, delay the unloading until it can be
  // invoked from EXE.
  return WTF::callOnMainThread(UnloadHelper, this);
}

WebKit::WebString TestWebWorkerHelper::DuplicateString(
    const WebKit::WebString& string) {
  return WebKit::WebString(string.data(), string.length());
}

void TestWebWorkerHelper::UnloadHelper(void* param) {
  TestWebWorkerHelper* this_ptr = static_cast<TestWebWorkerHelper*>(param);
  if (this_ptr->worker_count_)
    return;

#if defined(OS_WIN)
  if (this_ptr->module_) {
    FreeLibrary(this_ptr->module_);
    this_ptr->module_ = 0;
  }
#elif defined(OS_MACOSX)
  if (this_ptr->module_) {
    dlclose(this_ptr->module_);
    this_ptr->module_ = 0;
    this_ptr->CreateWebWorker_ = 0;
  }
#else
  NOTIMPLEMENTED();
#endif

  delete this_ptr;
  g_helper = NULL;
}
