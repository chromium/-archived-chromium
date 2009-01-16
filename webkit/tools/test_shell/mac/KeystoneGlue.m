// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "KeystoneGlue.h"

// Provide declarations of the Keystone registration bits needed here.  From
// KSRegistration.h.
typedef enum { kKSPathExistenceChecker } KSExistenceCheckerType;
@interface KSRegistration : NSObject
+ (id)registrationWithProductID:(NSString*)productID;
- (BOOL)registerWithVersion:(NSString*)version
       existenceCheckerType:(KSExistenceCheckerType)xctype
     existenceCheckerString:(NSString*)xc
            serverURLString:(NSString*)serverURLString;
- (void)setActive;
@end

@implementation KeystoneGlue

+ (void)registerWithKeystone {
  // Figure out who we are.
  NSBundle* mainBundle = [NSBundle mainBundle];
  NSString* bundleIdentifier = [mainBundle bundleIdentifier];
  NSDictionary* infoDictionary = [mainBundle infoDictionary];
  NSString* url = [infoDictionary objectForKey:@"KSUpdateURL"];
  NSString* version = [infoDictionary objectForKey:(id)kCFBundleVersionKey];
  if (!bundleIdentifier || !url || !version) {
    // If parameters required for Keystone are missing, don't use it.
    return;
  }

  // Load the KeystoneRegistration framework bundle.
  NSString* ksrPath =
      [[mainBundle privateFrameworksPath]
          stringByAppendingPathComponent:@"KeystoneRegistration.framework"];
  NSBundle* ksrBundle = [NSBundle bundleWithPath:ksrPath];
  [ksrBundle load];

  // Harness the KSRegistration class.
  Class ksrClass = [ksrBundle classNamed:@"KSRegistration"];
  KSRegistration* ksr = [ksrClass registrationWithProductID:bundleIdentifier];
  if (!ksr) {
    // Strictly speaking, this isn't necessary, because it's harmless to send
    // messages to nil.  However, if there really isn't a
    // KeystoneRegistration.framework or KSRegistration class, bailing out here
    // avoids setting up the timer that will only be able to perform no-ops.
    return;
  }

  // Keystone will asynchronously handle installation and registration as
  // needed.
  [ksr registerWithVersion:version
      existenceCheckerType:kKSPathExistenceChecker
    existenceCheckerString:[mainBundle bundlePath]
           serverURLString:url];

  // Set up hourly activity pings.
  [NSTimer scheduledTimerWithTimeInterval:60 * 60  // One hour
                                   target:self
                                 selector:@selector(markActive:)
                                 userInfo:ksr
                                  repeats:YES];
}

+ (void)markActive:(NSTimer*)timer {
  KSRegistration* ksr = [timer userInfo];
  [ksr setActive];
}

@end
