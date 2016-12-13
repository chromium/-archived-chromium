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
void EnableThemeSupportForRenderer(bool no_sandbox) {
  HWINSTA current = NULL;
  HWINSTA winsta0 = NULL;

  if (!no_sandbox) {
    current = ::GetProcessWindowStation();
    winsta0 = ::OpenWindowStationW(L"WinSta0", FALSE, GENERIC_READ);
    if (!winsta0 || !::SetProcessWindowStation(winsta0)) {
      // Could not set the alternate window station. There is a possibility
      // that the theme wont be correctly initialized on XP.
      NOTREACHED() << "Unable to switch to WinSt0";
    }
  }

  HWND window = ::CreateWindowExW(0, L"Static", L"", WS_POPUP | WS_DISABLED,
                                  CW_USEDEFAULT, 0, 0, 0,  HWND_MESSAGE, NULL, 
                                  ::GetModuleHandleA(NULL), NULL);
  if (!window) {
    DLOG(WARNING) << "failed to enable theme support";
  } else {
    ::DestroyWindow(window);
  }

  if (!no_sandbox) {
    // Revert the window station.
    if (!current || !::SetProcessWindowStation(current)) {
      // We failed to switch back to the secure window station. This might
      // confuse the renderer enough that we should kill it now.
      CHECK(false) << "Failed to restore alternate window station";
    }

    if (!::CloseWindowStation(winsta0)) {
      // We might be leaking a winsta0 handle.  This is a security risk, but
      // since we allow fail over to no desktop protection in low memory
      // condition, this is not a big risk.
      NOTREACHED();
    }
  }
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
  const CommandLine& command_line = parameters_.command_line_;
  bool no_sandbox = command_line.HasSwitch(switches::kNoSandbox);
  EnableThemeSupportForRenderer(no_sandbox);
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
