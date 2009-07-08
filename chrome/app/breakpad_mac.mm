// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/app/breakpad_mac.h"

#import <Foundation/Foundation.h>

#include "base/base_switches.h"
#import "base/basictypes.h"
#include "base/command_line.h"
#import "base/logging.h"
#import "base/scoped_nsautorelease_pool.h"
#include "base/sys_string_conversions.h"
#import "breakpad/src/client/mac/Framework/Breakpad.h"
#include "chrome/installer/util/google_update_settings.h"

extern "C" {

#if !defined(GOOGLE_CHROME_BUILD)

// If we aren't compiling as a branded build, then add dummy versions of the
// Breakpad functions so we don't have to link against Breakpad.

BreakpadRef BreakpadCreate(NSDictionary *parameters) {
  NOTREACHED();
  return NULL;
}

void BreakpadRelease(BreakpadRef ref) {
  NOTREACHED();
}

void BreakpadAddUploadParameter(BreakpadRef ref, NSString *key,
                                NSString *value) {
  NOTREACHED();
}

void BreakpadRemoveUploadParameter(BreakpadRef ref, NSString *key) {
  NOTREACHED();
}

#endif  // !defined(GOOGLE_CHROME_BUILD)

namespace {

BreakpadRef gBreakpadRef = NULL;

} // namespace

bool IsCrashReporterDisabled() {
  return gBreakpadRef == NULL;
}

void DestructCrashReporter() {
  if (gBreakpadRef) {
    BreakpadRelease(gBreakpadRef);
    gBreakpadRef = NULL;
  }
}

// Only called for a branded build of Chrome.app.
void InitCrashReporter() {
  DCHECK(gBreakpadRef == NULL);
  base::ScopedNSAutoreleasePool autorelease_pool;

  // Check for Send stats preference. If preference is not specifically turned
  // on then disable crash reporting.
  if (!GoogleUpdateSettings::GetCollectStatsConsent()) {
    LOG(WARNING) << "Breakpad disabled";
    return;
  }

  NSBundle* main_bundle = [NSBundle mainBundle];
  NSString* resource_path = [main_bundle resourcePath];

  NSDictionary* info_dictionary = [main_bundle infoDictionary];
  NSMutableDictionary *breakpad_config = [info_dictionary
                                             mutableCopy];

  // Tell Breakpad where inspector & crash_reporter are.
  NSString *inspector_location = [resource_path
      stringByAppendingPathComponent:@"crash_inspector"];
  NSString *reporter_bundle_location = [resource_path
      stringByAppendingPathComponent:@"crash_report_sender.app"];
  NSString *reporter_location = [[NSBundle
                                      bundleWithPath:reporter_bundle_location]
                                     executablePath];

  [breakpad_config setObject:inspector_location
                      forKey:@BREAKPAD_INSPECTOR_LOCATION];
  [breakpad_config setObject:reporter_location
                      forKey:@BREAKPAD_REPORTER_EXE_LOCATION];

  // Init breakpad
  BreakpadRef breakpad = NULL;
  breakpad = BreakpadCreate(breakpad_config);
  if (!breakpad) {
    LOG(ERROR) << "Breakpad init failed.";
    return;
  }

  // This needs to be set before calling SetCrashKeyValue().
  gBreakpadRef = breakpad;

  // Set breakpad MetaData values.
  // These values are added to the plist when building a branded Chrome.app.
  NSString* version_str = [info_dictionary objectForKey:@BREAKPAD_VERSION];
  SetCrashKeyValue(@"ver", version_str);
  NSString* prod_name_str = [info_dictionary objectForKey:@BREAKPAD_PRODUCT];
  SetCrashKeyValue(@"prod", prod_name_str);
  SetCrashKeyValue(@"plat", @"OS X");
}

void InitCrashProcessInfo() {
  if (gBreakpadRef == NULL) {
    return;
  }

  // Determine the process type.
  NSString *process_type = @"browser";
  const CommandLine& parsed_command_line = *CommandLine::ForCurrentProcess();
  std::wstring process_type_switch =
      parsed_command_line.GetSwitchValue(switches::kProcessType);
  if (!process_type_switch.empty()) {
    process_type = base::SysWideToNSString(process_type_switch);
  }

  // Store process type in crash dump.
  SetCrashKeyValue(@"ptype", process_type);
}

void SetCrashKeyValue(NSString* key, NSString* value) {
  // Comment repeated from header to prevent confusion:
  // IMPORTANT: On OS X, the key/value pairs are sent to the crash server
  // out of bounds and not recorded on disk in the minidump, this means
  // that if you look at the minidump file locally you won't see them!
  if (gBreakpadRef == NULL) {
    return;
  }

  BreakpadAddUploadParameter(gBreakpadRef, key, value);
}

void ClearCrashKeyValue(NSString* key) {
  if (gBreakpadRef == NULL) {
    return;
  }

  BreakpadRemoveUploadParameter(gBreakpadRef, key);
}

}

