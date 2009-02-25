// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/renderer_main_platform_delegate.h"

#include "base/debug_util.h"

#import <Foundation/Foundation.h>
#import <ApplicationServices/ApplicationServices.h>
#import <Cocoa/Cocoa.h>
extern "C" {
#include <sandbox.h>
}

#include "base/sys_info.h"
#include "chrome/common/chrome_switches.h"
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

#if 0

  // Note: by default, Cocoa is NOT thread safe.  Use of NSThreads
  // tells Cocoa to be MT-aware and create and use locks.  The
  // renderer process only uses Cocoa from the single renderer thread,
  // so we don't need to tell Cocoa we are using threads (even though,
  // oddly, we are using Cocoa from the non-main thread.)
  // The current limit of renderer processes is 20.  Brett states that
  // (despite comments to the contrary) when two tabs are using the
  // same renderer, we do NOT create a 2nd renderer thread in that
  // process.  Thus, we don't need to MT-aware Cocoa.
  // (Code and comments left here in case that changes.)
  if (![NSThread isMultiThreaded]) {
    NSString *string = @"";
    [NSThread detachNewThreadSelector:@selector(length)
                             toTarget:string
                           withObject:nil];
  }
#endif

  // Initialize Cocoa.  Without this call, drawing of native UI
  // elements (e.g. buttons) in WebKit will explode.
  [NSApplication sharedApplication];
}

void RendererMainPlatformDelegate::PlatformUninitialize() {
}

bool RendererMainPlatformDelegate::InitSandboxTests(bool no_sandbox) {
  return true;
}

bool RendererMainPlatformDelegate::EnableSandbox() {

  // This call doesn't work when the sandbox is enabled, the implementation
  // caches it's return value so we call it here and then future calls will
  // succeed.
  DebugUtil::BeingDebugged();

  // Cache the System info information, since we can't query certain attributes
  // with the Sandbox enabled.
  base::SysInfo::CacheSysInfo();

  // For the renderer, we give it a custom sandbox to lock down as tight as
  // possible, but still be able to draw.  If we're not a renderer process, it
  // usually means we're a unittest, so we use a pure compute sandbox instead.

  const char *sandbox_profile = kSBXProfilePureComputation;
  uint64_t sandbox_flags = SANDBOX_NAMED;

  if (parameters_.sandbox_info_.ProcessType() == switches::kRendererProcess) {
    NSString* sandbox_profile_path =
        [[NSBundle mainBundle] pathForResource:@"renderer" ofType:@"sb"];
    BOOL is_dir = NO;
    if (![[NSFileManager defaultManager] fileExistsAtPath:sandbox_profile_path
                                              isDirectory:&is_dir] || is_dir) {
      LOG(ERROR) << "Failed to find the sandbox profile on disk";
      return false;
    }
    sandbox_profile = [sandbox_profile_path fileSystemRepresentation];
    sandbox_flags = SANDBOX_NAMED_EXTERNAL;
  }

  char* error_buff = NULL;
  int error = sandbox_init(sandbox_profile, sandbox_flags,
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
