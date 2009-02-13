// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/renderer_main_platform_delegate.h"

#include "base/debug_util.h"

#include <ApplicationServices/ApplicationServices.h>
extern "C" {
#include <sandbox.h>
}

#include "base/sys_info.h"
#include "third_party/WebKit/WebKit/mac/WebCoreSupport/WebSystemInterface.h"

RendererMainPlatformDelegate::RendererMainPlatformDelegate(
    const MainFunctionParams& parameters)
        : parameters_(parameters) {
}

RendererMainPlatformDelegate::~RendererMainPlatformDelegate() {
}

void RendererMainPlatformDelegate::PlatformInitialize() {
  // Load WebKit system interfaces.
  InitWebCoreSystemInterface();

  // Warmup CG - without these calls these two functions won't work in the
  // sandbox.
  CGColorSpaceRef rgb_colorspace =
      CGColorSpaceCreateWithName(kCGColorSpaceGenericRGB);

  // Allocate a 1 byte image.
  char data[8];
  CGContextRef tmp = CGBitmapContextCreate(data, 1, 1, 8, 1*8,
                                           rgb_colorspace,
                                           kCGImageAlphaPremultipliedFirst |
                                           kCGBitmapByteOrder32Host);
  CGColorSpaceRelease(rgb_colorspace);
  CGContextRelease(tmp);
}

void RendererMainPlatformDelegate::PlatformUninitialize() {
}

bool RendererMainPlatformDelegate::InitSandboxTests(bool no_sandbox) {
  return true;
}

bool RendererMainPlatformDelegate::EnableSandbox() {

  // TODO(port): hack
  // With the sandbox on we don't have fonts in WebKit!
  return true;

  // This call doesn't work when the sandbox is enabled, the implementation
  // caches it's return value so we call it here and then future calls will
  // succeed.
  DebugUtil::BeingDebugged();

  // Cache the System info information, since we can't query certain attributes
  // with the Sandbox enabled.
  base::SysInfo::CacheSysInfo();

  char* error_buff = NULL;
  int error = sandbox_init(kSBXProfilePureComputation, SANDBOX_NAMED,
                           &error_buff);
  bool success = (error == 0 && error_buff == NULL);
  if (error == -1) {
    LOG(ERROR) << "Failed to Initialize Sandbox: " << error_buff;
  }
  sandbox_free_error(error_buff);
  return success;
}

void RendererMainPlatformDelegate::RunSandboxTests() {
  // TODO(port): Run sandbox unit test here.
}
