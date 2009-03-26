// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/app/breakpad_mac.h"

#import <objc/objc-class.h>
#import <Foundation/Foundation.h>

#import "base/basictypes.h"
#import "base/logging.h"
#import "base/scoped_nsautorelease_pool.h"

// TODO(jeremy): Remove this block once we include the breakpad sources
// in the public tree.
@interface GoogleBreakpadWrapper : NSObject
  +(void*)Create:(NSDictionary*) parameters;
  +(void)Release:(void*) ref;
@end
typedef void* GoogleBreakpadRef;

namespace {

// TODO(jeremy): On Windows we store the current URL when a process crashes
// we probably want to do the same on OS X.

GoogleBreakpadRef gBreakpadRef = NULL;

// Did the user optin for reporting stats.
bool IsStatsReportingAllowed() {
  NOTIMPLEMENTED();
  return true;
}

} // namespace

bool IsCrashReporterEnabled() {
  return gBreakpadRef == NULL;
}

void DestructCrashReporter() {
  if (gBreakpadRef) {
    Class breakpad_interface = objc_getClass("GoogleBreakpadWrapper");
    [breakpad_interface Release:gBreakpadRef];
    gBreakpadRef = NULL;
  }
}

void InitCrashReporter() {
  DCHECK(gBreakpadRef == NULL);
  base::ScopedNSAutoreleasePool autorelease_pool;

  // Check for Send stats preference. If preference is not specifically turned
  // on then disable crash reporting.
  if (!IsStatsReportingAllowed()) {
    LOG(WARNING) << "Breakpad disabled";
    return;
  }

  NSBundle* main_bundle = [NSBundle mainBundle];

  // Get location of breakpad.
  NSString* breakpadBundlePath = [[main_bundle privateFrameworksPath]
      stringByAppendingPathComponent:@"GoogleBreakpad.framework"];

  BOOL is_dir = NO;
  if (![[NSFileManager defaultManager] fileExistsAtPath:breakpadBundlePath
                                            isDirectory:&is_dir] || !is_dir) {
    return;
  }

  NSBundle* breakpad_bundle = [NSBundle bundleWithPath:breakpadBundlePath];
  if (![breakpad_bundle load]) {
     LOG(ERROR) << "Failed to load Breakpad framework.";
    return;
  }

  Class breakpad_interface = [breakpad_bundle
      classNamed:@"GoogleBreakpadWrapper"];

  if (!breakpad_interface) {
     LOG(ERROR) << "Failed to find GoogleBreakpadWrapper class.";
    return;
  }

  NSDictionary* info_dictionary = [main_bundle infoDictionary];
  GoogleBreakpadRef breakpad = 0;
  breakpad = [breakpad_interface Create:info_dictionary];
  if (!breakpad) {
    LOG(ERROR) << "Breakpad init failed.";
    return;
  }

  gBreakpadRef = breakpad;
}
