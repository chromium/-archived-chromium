// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/renderer_main_platform_delegate.h"

#include "base/debug_util.h"

// This is a no op class because we do not have a sandbox on linux.

RendererMainPlatformDelegate::RendererMainPlatformDelegate(
    const MainFunctionParams& parameters)
        : parameters_(parameters) {
}

RendererMainPlatformDelegate::~RendererMainPlatformDelegate() {
}

void RendererMainPlatformDelegate::PlatformInitialize() {
}

void RendererMainPlatformDelegate::PlatformUninitialize() {
}

bool RendererMainPlatformDelegate::InitSandboxTests(bool no_sandbox) {
  // We have no sandbox.
  // http://code.google.com/p/chromium/issues/detail?id=8081
  return true;
}

bool RendererMainPlatformDelegate::EnableSandbox() {
  // We have no sandbox.
  // http://code.google.com/p/chromium/issues/detail?id=8081
  return true;
}

void RendererMainPlatformDelegate::RunSandboxTests() {
  // We have no sandbox.
  // http://code.google.com/p/chromium/issues/detail?id=8081
}
