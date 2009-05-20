// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_APP_KEYSTONE_GLUE_H_
#define CHROME_APP_KEYSTONE_GLUE_H_

#import <Foundation/Foundation.h>

// Objects which request callbacks from KeystoneGlue (e.g. information
// on update availability) should implement this protocol.  All callbacks
// require the caller to be spinning in the runloop to happen.
@protocol KeystoneGlueCallbacks

// Callback when a checkForUpdate completes.
// |latestVersion| may be nil if not returned from the server.
// |latestVersion| is not a localizable string.
- (void)upToDateCheckCompleted:(BOOL)upToDate
                 latestVersion:(NSString*)latestVersion;

// Callback when a startUpdate completes.
// |successful| tells if the *check* was successful.  This does not
//   necessarily mean updates installed successfully.
// |installs| tells the number of updates that installed successfully
//   (typically 0 or 1).
- (void)updateCompleted:(BOOL)successful installs:(int)installs;

@end  // protocol KeystoneGlueCallbacks


// KeystoneGlue is an adapter around the KSRegistration class, allowing it to
// be used without linking directly against its containing KeystoneRegistration
// framework.  This is used in an environment where most builds (such as
// developer builds) don't want or need Keystone support and might not even
// have the framework available.  Enabling Keystone support in an application
// that uses KeystoneGlue is as simple as dropping
// KeystoneRegistration.framework in the application's Frameworks directory
// and providing the relevant information in its Info.plist.  KeystoneGlue
// requires that the KSUpdateURL key be set in the application's Info.plist,
// and that it contain a string identifying the update URL to be used by
// Keystone.

@class KSRegistration;

@interface KeystoneGlue : NSObject {
 @protected

  // Data for Keystone registration
  NSString* url_;
  NSString* productID_;
  NSString* version_;

  // And the Keystone registration itself, with the active timer
  KSRegistration* registration_;  // strong
  NSTimer* timer_;  // strong

  // Data for callbacks, all strong.  Deallocated (if needed) in a
  // NSNotificationCenter callback.
  NSObject<KeystoneGlueCallbacks>* startTarget_;
  NSObject<KeystoneGlueCallbacks>* checkTarget_;
}

// Return the default Keystone Glue object.
+ (id)defaultKeystoneGlue;

// Load KeystoneRegistration.framework if present, call into it to register
// with Keystone, and set up periodic activity pings.
- (void)registerWithKeystone;

// Check if updates are available.
// upToDateCheckCompleted:: called on target when done.
// Return NO if we could not start the check.
- (BOOL)checkForUpdate:(NSObject<KeystoneGlueCallbacks>*)target;

// Start an update.
// updateCompleted:: called on target when done.
// This cannot be cancelled.
// Return NO if we could not start the check.
- (BOOL)startUpdate:(NSObject<KeystoneGlueCallbacks>*)target;

@end  // KeystoneGlue


@interface KeystoneGlue (ExposedForTesting)

// Load any params we need for configuring Keystone.
- (void)loadParameters;

// Load the Keystone registration object.
// Return NO on failure.
- (BOOL)loadKeystoneRegistration;

- (void)stopTimer;

// Called when a checkForUpdate: notification completes.
- (void)checkComplete:(NSNotification *)notification;

// Called when a startUpdate: notification completes.
- (void)startUpdateComplete:(NSNotification *)notification;

@end  // KeystoneGlue (ExposedForTesting)

#endif  // CHROME_APP_KEYSTONE_GLUE_H_
