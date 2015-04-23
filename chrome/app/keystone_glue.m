// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "keystone_glue.h"

@interface KeystoneGlue(Private)

// Called periodically to announce activity by pinging the Keystone server.
- (void)markActive:(NSTimer*)timer;

@end


// Provide declarations of the Keystone registration bits needed here.  From
// KSRegistration.h.
typedef enum { kKSPathExistenceChecker } KSExistenceCheckerType;

NSString *KSRegistrationCheckForUpdateNotification =
    @"KSRegistrationCheckForUpdateNotification";
NSString *KSRegistrationStatusKey = @"Status";
NSString *KSRegistrationVersionKey = @"Version";

NSString *KSRegistrationStartUpdateNotification =
    @"KSRegistrationStartUpdateNotification";
NSString *KSUpdateCheckSuccessfulKey = @"CheckSuccessful";
NSString *KSUpdateCheckSuccessfullyInstalledKey = @"SuccessfullyInstalled";

@interface KSRegistration : NSObject
+ (id)registrationWithProductID:(NSString*)productID;
- (BOOL)registerWithVersion:(NSString*)version
       existenceCheckerType:(KSExistenceCheckerType)xctype
     existenceCheckerString:(NSString*)xc
            serverURLString:(NSString*)serverURLString;
- (void)setActive;
- (void)checkForUpdate;
- (void)startUpdate;
@end


@implementation KeystoneGlue

+ (id)defaultKeystoneGlue {
  // TODO(jrg): rename this file to .mm so I can use C++ and
  // make this type a base::SingletonObjC<KeystoneGlue>.
  static KeystoneGlue* sDefaultKeystoneGlue = nil;  // leaked

  if (sDefaultKeystoneGlue == nil) {
    sDefaultKeystoneGlue = [[KeystoneGlue alloc] init];
    [sDefaultKeystoneGlue loadParameters];
    if (![sDefaultKeystoneGlue loadKeystoneRegistration]) {
      [sDefaultKeystoneGlue release];
      sDefaultKeystoneGlue = nil;
    }
  }
  return sDefaultKeystoneGlue;
}

- (void)dealloc {
  [url_ release];
  [productID_ release];
  [version_ release];
  [registration_ release];
  [super dealloc];
}

- (NSDictionary*)infoDictionary {
  return [[NSBundle mainBundle] infoDictionary];
}

- (void)loadParameters {
  NSDictionary* infoDictionary = [self infoDictionary];
  NSString* url = [infoDictionary objectForKey:@"KSUpdateURL"];
  NSString* product = [infoDictionary objectForKey:@"KSProductID"];
  if (product == nil) {
    product = [[NSBundle mainBundle] bundleIdentifier];
  }
  NSString* version = [infoDictionary objectForKey:@"KSVersion"];
  if (!product || !url || !version) {
    // If parameters required for Keystone are missing, don't use it.
    return;
  }

  url_ = [url retain];
  productID_ = [product retain];
  version_ = [version retain];
}

- (BOOL)loadKeystoneRegistration {
  if (!productID_ || !url_ || !version_)
    return NO;

  // Load the KeystoneRegistration framework bundle.
  NSBundle* mainBundle = [NSBundle mainBundle];
  NSString* ksrPath =
      [[mainBundle privateFrameworksPath]
          stringByAppendingPathComponent:@"KeystoneRegistration.framework"];
  NSBundle* ksrBundle = [NSBundle bundleWithPath:ksrPath];
  [ksrBundle load];

  // Harness the KSRegistration class.
  Class ksrClass = [ksrBundle classNamed:@"KSRegistration"];
  KSRegistration* ksr = [ksrClass registrationWithProductID:productID_];
  if (!ksr)
    return NO;

  registration_ = [ksr retain];
  return YES;
}

- (void)registerWithKeystone {
  [registration_ registerWithVersion:version_
                existenceCheckerType:kKSPathExistenceChecker
              existenceCheckerString:[[NSBundle mainBundle] bundlePath]
                     serverURLString:url_];

  // Mark an active RIGHT NOW; don't wait an hour for the first one.
  [registration_ setActive];

  // Set up hourly activity pings.
  timer_ = [NSTimer scheduledTimerWithTimeInterval:60 * 60  // One hour
                                            target:self
                                          selector:@selector(markActive:)
                                          userInfo:registration_
                                           repeats:YES];
}

- (void)stopTimer {
  [timer_ invalidate];
}

- (void)markActive:(NSTimer*)timer {
  KSRegistration* ksr = [timer userInfo];
  [ksr setActive];
}

- (void)checkComplete:(NSNotification *)notification {
  NSDictionary *userInfo = [notification userInfo];
  BOOL updatesAvailable = [[userInfo objectForKey:KSRegistrationStatusKey]
                            boolValue];
  NSString *latestVersion = [userInfo objectForKey:KSRegistrationVersionKey];

  [checkTarget_ upToDateCheckCompleted:updatesAvailable
                         latestVersion:latestVersion];
  [checkTarget_ release];
  checkTarget_ = nil;

  NSNotificationCenter* center = [NSNotificationCenter defaultCenter];
  [center removeObserver:self
                    name:KSRegistrationCheckForUpdateNotification
                  object:nil];
}

- (BOOL)checkForUpdate:(NSObject<KeystoneGlueCallbacks>*)target {
  if (registration_ == nil)
    return NO;
  NSNotificationCenter* center = [NSNotificationCenter defaultCenter];
  [center addObserver:self
             selector:@selector(checkComplete:)
                 name:KSRegistrationCheckForUpdateNotification
               object:nil];
  checkTarget_ = [target retain];
  [registration_ checkForUpdate];
  return YES;
}

- (void)startUpdateComplete:(NSNotification *)notification {
  NSDictionary *userInfo = [notification userInfo];
  BOOL checkSuccessful = [[userInfo objectForKey:KSUpdateCheckSuccessfulKey]
                           boolValue];
  int installs = [[userInfo objectForKey:KSUpdateCheckSuccessfullyInstalledKey]
                   intValue];

  [startTarget_ updateCompleted:checkSuccessful installs:installs];
  [startTarget_ release];
  startTarget_ = nil;

  NSNotificationCenter* center = [NSNotificationCenter defaultCenter];
  [center removeObserver:self
                    name:KSRegistrationStartUpdateNotification
                  object:nil];
}

- (BOOL)startUpdate:(NSObject<KeystoneGlueCallbacks>*)target {
  if (registration_ == nil)
    return NO;
  NSNotificationCenter* center = [NSNotificationCenter defaultCenter];
  [center addObserver:self
             selector:@selector(startUpdateComplete:)
                 name:KSRegistrationStartUpdateNotification
               object:nil];
  startTarget_ = [target retain];
  [registration_ startUpdate];
  return YES;
}

@end
