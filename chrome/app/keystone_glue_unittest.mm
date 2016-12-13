// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Foundation/Foundation.h>
#import <objc/objc-class.h>

#import "chrome/app/keystone_glue.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

@interface FakeGlueRegistration : NSObject
@end


@implementation FakeGlueRegistration
- (void)checkForUpdate { }
- (void)startUpdate { }
@end


@interface FakeKeystoneGlue : KeystoneGlue<KeystoneGlueCallbacks> {
 @public
  BOOL upToDate_;
  NSString *latestVersion_;
  BOOL successful_;
  int installs_;
}
@end


@implementation FakeKeystoneGlue

- (id)init {
  if ((self = [super init])) {
    // some lies
    upToDate_ = YES;
    latestVersion_ = @"foo bar";
    successful_ = YES;
    installs_ = 1010101010;
  }
  return self;
}

// For mocking
- (NSDictionary*)infoDictionary {
  NSDictionary* dict = [NSDictionary dictionaryWithObjectsAndKeys:
                                     @"http://foo.bar", @"KSUpdateURL",
                                     @"com.google.whatever", @"KSProductID",
                                     @"0.0.0.1", @"KSVersion",
                                     nil];
  return dict;
}

// For mocking
- (BOOL)loadKeystoneRegistration {
  return YES;
}

// Confirms certain things are happy
- (BOOL)dictReadCorrectly {
  return ([url_ isEqual:@"http://foo.bar"] &&
          [productID_ isEqual:@"com.google.whatever"] &&
          [version_ isEqual:@"0.0.0.1"]);
}

// Confirms certain things are happy
- (BOOL)hasATimer {
  return timer_ ? YES : NO;
}

- (void)upToDateCheckCompleted:(BOOL)upToDate
                 latestVersion:(NSString*)latestVersion {
  upToDate_ = upToDate;
  latestVersion_ = latestVersion;
}

- (void)updateCompleted:(BOOL)successful installs:(int)installs {
  successful_ = successful;
  installs_ = installs;
}

- (void)addFakeRegistration {
  registration_ = [[FakeGlueRegistration alloc] init];
}

// Confirm we look like callbacks with nil NSNotifications
- (BOOL)confirmCallbacks {
  return (!upToDate_ &&
          (latestVersion_ == nil) &&
          !successful_ &&
          (installs_ == 0));
}

@end


namespace {

class KeystoneGlueTest : public PlatformTest {
};

TEST_F(KeystoneGlueTest, BasicGlobalCreate) {
  // Allow creation of a KeystoneGlue by mocking out a few calls
  SEL ids = @selector(infoDictionary);
  IMP oldInfoImp_ = [[KeystoneGlue class] instanceMethodForSelector:ids];
  IMP newInfoImp_ = [[FakeKeystoneGlue class] instanceMethodForSelector:ids];
  Method infoMethod_ = class_getInstanceMethod([KeystoneGlue class], ids);
  method_setImplementation(infoMethod_, newInfoImp_);

  SEL lks = @selector(loadKeystoneRegistration);
  IMP oldLoadImp_ = [[KeystoneGlue class] instanceMethodForSelector:lks];
  IMP newLoadImp_ = [[FakeKeystoneGlue class] instanceMethodForSelector:lks];
  Method loadMethod_ = class_getInstanceMethod([KeystoneGlue class], lks);
  method_setImplementation(loadMethod_, newLoadImp_);

  KeystoneGlue *glue = [KeystoneGlue defaultKeystoneGlue];
  ASSERT_TRUE(glue);

  // Fix back up the class to the way we found it.
  method_setImplementation(infoMethod_, oldInfoImp_);
  method_setImplementation(loadMethod_, oldLoadImp_);
}

TEST_F(KeystoneGlueTest, BasicUse) {
  FakeKeystoneGlue* glue = [[[FakeKeystoneGlue alloc] init] autorelease];
  [glue loadParameters];
  ASSERT_TRUE([glue dictReadCorrectly]);

  // Likely returns NO in the unit test, but call it anyway to make
  // sure it doesn't crash.
  [glue loadKeystoneRegistration];

  // Confirm we start up an active timer
  [glue registerWithKeystone];
  ASSERT_TRUE([glue hasATimer]);
  [glue stopTimer];

  ASSERT_TRUE(![glue checkForUpdate:glue] && ![glue startUpdate:glue]);

  // Brief exercise of callbacks
  [glue addFakeRegistration];
  ASSERT_TRUE([glue checkForUpdate:glue]);
  [glue checkComplete:nil];
  ASSERT_TRUE([glue startUpdate:glue]);
  [glue startUpdateComplete:nil];
  ASSERT_TRUE([glue confirmCallbacks]);
}

}  // namespace
