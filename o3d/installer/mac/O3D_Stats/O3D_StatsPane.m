//
//  O3D_StatsPane.m
//  O3D_Stats
//

// @@REWRITE(insert c-copyright)
// @@REWRITE(delete-start)
//  Created by Matthew Vosburgh on 4/14/09.
//  Copyright (c) 2009 Google Inc. All rights reserved.
// @@REWRITE(delete-end)

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
