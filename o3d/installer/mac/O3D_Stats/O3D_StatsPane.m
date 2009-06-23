//
//  O3D_StatsPane.m
//  O3D_Stats
//

/*
 * Copyright 2009, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "O3D_StatsPane.h"

#define kPrefString @"O3D_STATS"


@implementation O3D_StatsPane

// Read a pref string for current user, but global to all apps.
static NSString* ReadGlobalPreferenceString(NSString *key) {
  return (NSString *)CFPreferencesCopyValue((CFStringRef)key,
                                            kCFPreferencesAnyApplication,
                                            kCFPreferencesCurrentUser,
                                            kCFPreferencesCurrentHost);
}


// Write a pref string for the current user, but global to all apps.
static void WriteGlobalPreferenceString(NSString *key,
                                        NSString *value) {
  CFPreferencesSetValue((CFStringRef)key,
                        (CFPropertyListRef)value,
                        kCFPreferencesAnyApplication,
                        kCFPreferencesCurrentUser,
                        kCFPreferencesCurrentHost);

  CFPreferencesSynchronize(kCFPreferencesAnyApplication,
                           kCFPreferencesCurrentUser,
                           kCFPreferencesCurrentHost);
}


- (NSString *)title {
  NSBundle *bundle = [NSBundle bundleForClass:[self class]];

  return [bundle localizedStringForKey:@"PaneTitle"
                                 value:nil
                                 table:nil];
}


- (BOOL)shouldExitPane:(InstallerSectionDirection)dir {
  BOOL statsOn = [statsRadioMatrix_ selectedCell] == statsOnRadio_;

  WriteGlobalPreferenceString(kPrefString, statsOn ? @"YES" : @"NO");
  return YES;
}

- (void)didEnterPane:(InstallerSectionDirection)dir {
  NSString *value = ReadGlobalPreferenceString(kPrefString);
  BOOL statsOn = ![value isEqualToString:@"NO"];

  [statsRadioMatrix_ selectCell: statsOn ? statsOnRadio_ : statsOffRadio_];
}


@end
