// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_PLATFORM_TEST_H_
#define BASE_PLATFORM_TEST_H_

#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_MACOSX)
#ifdef __OBJC__
@class NSAutoreleasePool;
#else
class NSAutoreleasePool;
#endif

// The purpose of this class us to provide a hook for platform-specific
// SetUp and TearDown across unit tests.  For example, on the Mac, it
// creates and releases an outer AutoreleasePool for each test.  For now, it's
// only implemented on the Mac.  To enable this for another platform, just
// adjust the #ifdefs and add a platform_test_<platform>.cc implementation file.
class PlatformTest : public testing::Test {
 protected:  
  virtual void SetUp();
  virtual void TearDown();

 private:
  NSAutoreleasePool* pool_;
};
#else
typedef testing::Test PlatformTest;
#endif // OS_MACOSX

#endif // BASE_PLATFORM_TEST_H_


