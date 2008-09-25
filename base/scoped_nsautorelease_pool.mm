// Copyright (c) 2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/scoped_nsautorelease_pool.h"

#import <Foundation/Foundation.h>

namespace base {

ScopedNSAutoreleasePool::ScopedNSAutoreleasePool()
    : autorelease_pool_([[NSAutoreleasePool alloc] init]) {
}

ScopedNSAutoreleasePool::~ScopedNSAutoreleasePool() {
  [autorelease_pool_ drain];
}

}  // namespace base
