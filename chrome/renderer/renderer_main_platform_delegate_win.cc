// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/renderer_main_platform_delegate.h"

#include "base/command_line.h"
#include "base/gfx/native_theme.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/injection_test_dll.h"
#include "sandbox/src/sandbox.h"

namespace {

// In order to have Theme support, we need to connect to the theme service.
// This needs to be done before we lock down the renderer. Officially this
// can be done with OpenThemeData() but it fails unless you pass a valid
// window at least the first time. Interestingly, the very act of creating a
// window also sets the connection to the theme service.
void EnableThemeSupportForRenderer() {
  HWND window = ::CreateWindowExW(0, L"Static", L"", WS_POPUP | WS_DISABLED,
                                  CW_USEDEFAULT, 0, 0, 0,  HWND_MESSAGE, NULL, 
                                  ::GetModuleHandleA(NULL), NULL);
  if (!window) {
    DLOG(WARNING) << "failed to enable theme support";
    return;
  }
  ::DestroyWindow(window);
}

}  // namespace

RendererMainPlatformDelegate::RendererMainPlatformDelegate(
    const MainFunctionParams& parameters)
        : parameters_(parameters),
          sandbox_test_module_(NULL) {
}

RendererMainPlatformDelegate::~RendererMainPlatformDelegate() {
}

void RendererMainPlatformDelegate::PlatformInitialize() {
  // Be mindful of what resources you acquire here. They can be used by
  // malicious code if the renderer gets compromised.
  EnableThemeSupportForRenderer();
}

void RendererMainPlatformDelegate::PlatformUninitialize() {
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
