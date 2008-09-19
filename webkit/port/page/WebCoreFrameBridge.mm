// Copyright (c) 2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// WebCoreFrameBridge is an ObjC bridge between WebCore and WebKit, which we
// probably don't need to use for Chromium. If we do, it will probably differ
// substantially from the Apple implementation since we won't have the JS
// bindings available.
//
// We do, however, need to stub this class because ImageMac needs to find the
// application bundle so it can load default image resources. It does this
// by finding the bundle of a known class, and it chooses to use
// WebCoreFrameBridge. As a result, we have to provide linkage of at least
// the classname so that the use in ImageMac resolves. 

@implementation WebCoreFrameBridge

// nothing needed....

@end
