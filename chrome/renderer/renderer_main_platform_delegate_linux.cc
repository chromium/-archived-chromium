// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/renderer_main_platform_delegate.h"

#include <stdlib.h>

#include "base/debug_util.h"
#include "base/eintr_wrapper.h"

// This is a no op class because we do not have a sandbox on linux.

RendererMainPlatformDelegate::RendererMainPlatformDelegate(
    const MainFunctionParams& parameters)
        : parameters_(parameters) {
}

RendererMainPlatformDelegate::~RendererMainPlatformDelegate() {
}

extern void SkiaFontConfigUseIPCImplementation(int fd);
extern void SkiaFontConfigUseDirectImplementation();

void RendererMainPlatformDelegate::PlatformInitialize() {
}

void RendererMainPlatformDelegate::PlatformUninitialize() {
}

bool RendererMainPlatformDelegate::InitSandboxTests(bool no_sandbox) {
  // Our sandbox support is in the very early stages
  // http://code.google.com/p/chromium/issues/detail?id=8081
  return true;
}

bool RendererMainPlatformDelegate::EnableSandbox() {
  // Our sandbox support is in the very early stages
  // http://code.google.com/p/chromium/issues/detail?id=8081

  const char* const sandbox_fd_string = getenv("SBX_D");
  if (sandbox_fd_string) {
    // The SUID sandbox sets this environment variable to a file descriptor
    // over which we can signal that we have completed our startup and can be
    // chrooted.

    char* endptr;
    const long fd_long = strtol(sandbox_fd_string, &endptr, 10);
    if (!*sandbox_fd_string || *endptr || fd_long < 0 || fd_long > INT_MAX)
      return false;
    const int fd = fd_long;

    static const char kChrootMe = 'C';
    static const char kChrootMeSuccess = 'O';

    if (HANDLE_EINTR(write(fd, &kChrootMe, 1)) != 1)
      return false;

    char reply;
    if (HANDLE_EINTR(read(fd, &reply, 1)) != 1)
      return false;
    if (reply != kChrootMeSuccess)
      return false;
    if (chdir("/") == -1)
      return false;

    static const int kMagicSandboxIPCDescriptor = 5;
    SkiaFontConfigUseIPCImplementation(kMagicSandboxIPCDescriptor);
  } else {
    SkiaFontConfigUseDirectImplementation();
  }

  return true;
}

void RendererMainPlatformDelegate::RunSandboxTests() {
  // Our sandbox support is in the very early stages
  // http://code.google.com/p/chromium/issues/detail?id=8081
}
