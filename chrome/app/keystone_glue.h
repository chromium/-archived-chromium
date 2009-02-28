// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_APP_KEYSTONE_GLUE_H_
#define CHROME_APP_KEYSTONE_GLUE_H_

#import <Foundation/Foundation.h>

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

@interface KeystoneGlue : NSObject

// Load KeystoneRegistration.framework if present, call into it to register
// with Keystone, and set up periodic activity pings.
+ (void)registerWithKeystone;

// Called periodically to announce activity by pinging the Keystone server.
+ (void)markActive:(NSTimer*)timer;

@end

#endif  // CHROME_APP_KEYSTONE_GLUE_H_
