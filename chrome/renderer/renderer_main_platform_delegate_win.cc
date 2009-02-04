// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/renderer_main_platform_delegate.h"

#include <objbase.h>

#include "base/command_line.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/injection_test_dll.h"
#include "sandbox/src/sandbox.h"

RendererMainPlatformDelegate::RendererMainPlatformDelegate(
    const MainFunctionParams& parameters)
        : parameters_(parameters),
          sandbox_test_module_(NULL) {
}

RendererMainPlatformDelegate::~RendererMainPlatformDelegate() {
}

void RendererMainPlatformDelegate::PlatformInitialize() {
  CoInitialize(NULL);
}

void RendererMainPlatformDelegate::PlatformUninitialize() {
  CoUninitialize();
}

bool RendererMainPlatformDelegate::InitSandboxTests(bool no_sandbox) {
  const CommandLine& command_line = parameters_.command_line_;

  DLOG(INFO) << "Started renderer with " << command_line.command_line_string();

  sandbox::TargetServices* target_services =
      parameters_.sandbox_info_.TargetServices();

  if (target_services && !no_sandbox) {
      std::wstring test_dll_name =
          command_line.GetSwitchValue(switches::kTestSandbox);
    if (!test_dll_name.empty()) {
      sandbox_test_module_ = LoadLibrary(test_dll_name.c_str());
      DCHECK(sandbox_test_module_);
      if (!sandbox_test_module_) {
        return false;
      }
    }
  }
  return true;
}

bool RendererMainPlatformDelegate::EnableSandbox() {
  sandbox::TargetServices* target_services =
      parameters_.sandbox_info_.TargetServices();

  if (target_services) {
    target_services->LowerToken();
    return true;
  }
  return false;
}

void RendererMainPlatformDelegate::RunSandboxTests() {
  if (sandbox_test_module_) {
    RunRendererTests run_security_tests =
        reinterpret_cast<RunRendererTests>(GetProcAddress(sandbox_test_module_,
                                                          kRenderTestCall));
    DCHECK(run_security_tests);
    if (run_security_tests) {
      int test_count = 0;
      DLOG(INFO) << "Running renderer security tests";
      BOOL result = run_security_tests(&test_count);
      CHECK(result) << "Test number " << test_count << " has failed.";
    }
  }
}
