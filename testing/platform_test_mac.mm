// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform_test.h"

#import <Foundation/Foundation.h>

void PlatformTest::SetUp() {
  pool_ = [[NSAutoreleasePool alloc] init];
}

void PlatformTest::TearDown() {
  [pool_ drain];
}
