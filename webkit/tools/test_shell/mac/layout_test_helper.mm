// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <AppKit/AppKit.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>

// This is a simple helper app that changes the color sync profile to the
// generic profile and back when done.  This program is managed by the layout
// test script, so it can do the job for multiple test shells while they are
// running layout tests.

static CMProfileRef gUsersColorProfile = NULL;

static void SaveCurrentColorProfile(void) {
  CGDirectDisplayID displayID = CGMainDisplayID();
  CMProfileRef previousProfile;
  CMError error = CMGetProfileByAVID((UInt32)displayID, &previousProfile);
  if (error) {
    NSLog(@"failed to get the current color profile, pixmaps won't match. "
          @"Error: %d", (int)error);
  } else {
    gUsersColorProfile = previousProfile;
  }
}

static void InstallLayoutTestColorProfile(void) {
  // To make sure we get consistent colors (not dependent on the Main display),
  // we force the generic rgb color profile.  This cases a change the user can
  // see.

  CGDirectDisplayID displayID = CGMainDisplayID();
  NSColorSpace *genericSpace = [NSColorSpace genericRGBColorSpace];
  CMProfileRef genericProfile = (CMProfileRef)[genericSpace colorSyncProfile];
  CMError error = CMSetProfileByAVID((UInt32)displayID, genericProfile);
  if (error) {
    NSLog(@"failed install the generic color profile, pixmaps won't match. "
          @"Error: %d", (int)error);
  }
}

static void RestoreUsersColorProfile(void) {
  if (gUsersColorProfile) {
    CGDirectDisplayID displayID = CGMainDisplayID();
    CMError error = CMSetProfileByAVID((UInt32)displayID, gUsersColorProfile);
    CMCloseProfile(gUsersColorProfile);
    if (error) {
      NSLog(@"Failed to restore color profile, use System Preferences -> "
            @"Displays -> Color to reset. Error: %d", (int)error);
    }
    gUsersColorProfile = NULL;
  }
}

static void SimpleSignalHandler(int sig) {
  // Try to restore the color profile and try to go down cleanly
  RestoreUsersColorProfile();
  exit(128 + sig);
}

int main(int argc, char *argv[]) {
  NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];

  // Hooks the ways we might get told to clean up...
  signal(SIGINT, SimpleSignalHandler);
  signal(SIGHUP, SimpleSignalHandler);
  signal(SIGTERM, SimpleSignalHandler);

  // Save off the current profile, and then install the layout test profile.
  SaveCurrentColorProfile();
  InstallLayoutTestColorProfile();

  // Let the script know we're ready
  printf("ready\n");
  fflush(stdout);

  // Wait for any key (or signal)
  getchar();

  // Restore the profile
  RestoreUsersColorProfile();

  [pool release];
  return 0;
}
