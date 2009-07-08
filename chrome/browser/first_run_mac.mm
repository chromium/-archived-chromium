// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/first_run.h"

#import "base/scoped_nsobject.h"
#include "base/sys_string_conversions.h"
#import "chrome/app/breakpad_mac.h"
#import "chrome/browser/cocoa/first_run_dialog.h"
#include "chrome/installer/util/google_update_constants.h"
#include "chrome/installer/util/google_update_settings.h"

// static
bool FirstRun::IsChromeFirstRun() {
#if defined(GOOGLE_CHROME_BUILD)
  // Use presence of kRegUsageStatsField key as an indicator of whether or not
  // this is the first run.
  // See chrome/browser/google_update_settings_mac.mm for details on why we use
  // the defualts dictionary here.
  NSUserDefaults* std_defaults = [NSUserDefaults standardUserDefaults];
  NSDictionary* defaults_dict = [std_defaults dictionaryRepresentation];
  NSString* collect_stats_key = base::SysWideToNSString(
      google_update::kRegUsageStatsField);

  bool not_in_dict = [defaults_dict objectForKey:collect_stats_key] == nil;
  return not_in_dict;
#else
  return false; // no first run UI for Chromium builds
#endif  // defined(GOOGLE_CHROME_BUILD)
}

bool OpenFirstRunDialog(Profile* profile, ProcessSingleton* process_singleton) {
// OpenFirstRunDialog is a no-op on non-branded builds.
#if defined(GOOGLE_CHROME_BUILD)
  // Breakpad should not be enabled on first run until the user has explicitly
  // opted-into stats.
  // TODO: The behavior we probably want here is to enable Breakpad on first run
  // but display a confirmation dialog before sending a crash report so we
  // respect a user's privacy while still getting any crashes that might happen
  // before this point.  Then remove the need for that dialog here.
  DCHECK(IsCrashReporterDisabled());

  scoped_nsobject<FirstRunDialogController> dialog(
      [[FirstRunDialogController alloc] init]);

  bool stats_enabled = [dialog.get() Show];

  GoogleUpdateSettings::SetCollectStatsConsent(stats_enabled);

  // Breakpad is normally enabled very early in the startup process,
  // however, on the first run it's off by default.  If the user opts-in to
  // stats, enable breakpad.
  if (stats_enabled) {
    InitCrashReporter();
    InitCrashProcessInfo();
  }
#endif  // defined(GOOGLE_CHROME_BUILD)
  return true;
}
