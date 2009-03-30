// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/app/breakpad_mac.h"

#import <dlfcn.h>
#import <Foundation/Foundation.h>

#import "base/basictypes.h"
#import "base/logging.h"
#import "base/scoped_nsautorelease_pool.h"

// For definition of SetActiveRendererURL().
#import "chrome/renderer/renderer_logging.h"
#import "googleurl/src/gurl.h"

// TODO(jeremy): On Windows we store the current URL when a process crashes
// we probably want to do the same on OS X.

namespace {

// TODO(jeremy): Remove this block once we include the breakpad sources
// in the public tree.
typedef void* GoogleBreakpadRef;

typedef void (*GoogleBreakpadSetKeyValuePtr) (GoogleBreakpadRef, NSString*,
                                              NSString*);
typedef void (*GoogleBreakpadRemoveKeyValuePtr) (GoogleBreakpadRef, NSString*);
typedef GoogleBreakpadRef (*GoogleBreakPadCreatePtr) (NSDictionary*);
typedef void (*GoogleBreakPadReleasePtr) (GoogleBreakpadRef);


GoogleBreakpadRef gBreakpadRef = NULL;
GoogleBreakPadCreatePtr gBreakPadCreateFunc = NULL;
GoogleBreakPadReleasePtr gBreakPadReleaseFunc = NULL;
GoogleBreakpadSetKeyValuePtr gBreakpadSetKeyValueFunc = NULL;
GoogleBreakpadRemoveKeyValuePtr gBreakpadRemoveKeyValueFunc = NULL;

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
    DCHECK(gBreakPadReleaseFunc != NULL);
    gBreakPadReleaseFunc(gBreakpadRef);
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

  // Retrieve Breakpad interface functions.
  gBreakPadCreateFunc = reinterpret_cast<GoogleBreakPadCreatePtr>(
      dlsym(RTLD_DEFAULT, "GoogleBreakpadCreate"));
  gBreakPadReleaseFunc = reinterpret_cast<GoogleBreakPadReleasePtr>(
      dlsym(RTLD_DEFAULT, "GoogleBreakpadRelease"));
  gBreakpadSetKeyValueFunc = reinterpret_cast<GoogleBreakpadSetKeyValuePtr>(
      dlsym(RTLD_DEFAULT, "GoogleBreakpadSetKeyValue"));
  gBreakpadRemoveKeyValueFunc =
      reinterpret_cast<GoogleBreakpadRemoveKeyValuePtr>(
          dlsym(RTLD_DEFAULT, "GoogleBreakpadRemoveKeyValue"));

  if (!gBreakPadCreateFunc || !gBreakPadReleaseFunc
      || !gBreakpadSetKeyValueFunc || !gBreakpadRemoveKeyValueFunc) {
    LOG(ERROR) << "Failed to find Breakpad wrapper classes.";
    return;
  }

  NSDictionary* info_dictionary = [main_bundle infoDictionary];
  GoogleBreakpadRef breakpad = NULL;
  breakpad = gBreakPadCreateFunc(info_dictionary);
  if (!breakpad) {
    LOG(ERROR) << "Breakpad init failed.";
    return;
  }

  // TODO(jeremy): determine whether we're running in the browser or
  // renderer processes.
  bool is_renderer = false;

  // This needs to be set before calling SetCrashKeyValue().
  gBreakpadRef = breakpad;

  // Set breakpad MetaData values.
  NSString* version_str = [info_dictionary objectForKey:@"CFBundleVersion"];
  SetCrashKeyValue(@"ver", version_str);
  NSString* prod_name_str = [info_dictionary objectForKey:@"CFBundleExecutable"];
  SetCrashKeyValue(@"prod", prod_name_str);
  SetCrashKeyValue(@"plat", @"OS X");
  NSString *product_type = is_renderer ? @"renderer" : @"browser";
  SetCrashKeyValue(@"ptype", product_type);
}

void SetCrashKeyValue(NSString* key, NSString* value) {
  // Comment repeated from header to prevent confusion:
  // IMPORTANT: On OS X, the key/value pairs are sent to the crash server
  // out of bounds and not recorded on disk in the minidump, this means
  // that if you look at the minidump file locally you won't see them!
  if (gBreakpadRef == NULL) {
    return;
  }

  DCHECK(gBreakpadSetKeyValueFunc != NULL);
  gBreakpadSetKeyValueFunc(gBreakpadRef, key, value);
}

void ClearCrashKeyValue(NSString* key) {
  if (gBreakpadRef == NULL) {
    return;
  }

  DCHECK(gBreakpadRemoveKeyValueFunc != NULL);
  gBreakpadRemoveKeyValueFunc(gBreakpadRef, key);
}
