// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include "base/file_path.h"
#include "base/mac_util.h"
#include "base/path_service.h"
#include "chrome/browser/cocoa/nsimage_cache.h"
#include "chrome/common/mac_app_names.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class NSImageCacheTest : public testing::Test {
 public:
  NSImageCacheTest() {
    // Look in the Chromium app bundle for resources.
    FilePath path;
    PathService::Get(base::DIR_EXE, &path);
    path = path.AppendASCII(MAC_BROWSER_APP_NAME);
    mac_util::SetOverrideAppBundlePath(path);
  }
  virtual ~NSImageCacheTest() {
    mac_util::SetOverrideAppBundle(nil);
  }
};

TEST_F(NSImageCacheTest, LookupFound) {
  EXPECT_TRUE(nsimage_cache::ImageNamed(@"back.pdf") != nil)
      << "Failed to find the toolbar image?";
}

TEST_F(NSImageCacheTest, LookupCached) {
  EXPECT_EQ(nsimage_cache::ImageNamed(@"back.pdf"),
            nsimage_cache::ImageNamed(@"back.pdf"))
    << "Didn't get the same NSImage back?";
}

TEST_F(NSImageCacheTest, LookupMiss) {
  EXPECT_TRUE(nsimage_cache::ImageNamed(@"should_not.exist") == nil)
      << "There shouldn't be an image with this name?";
}

TEST_F(NSImageCacheTest, LookupFoundAndClear) {
  NSImage *first = nsimage_cache::ImageNamed(@"back.pdf");
  EXPECT_TRUE(first != nil)
      << "Failed to find the toolbar image?";
  nsimage_cache::Clear();
  EXPECT_NE(first, nsimage_cache::ImageNamed(@"back.pdf"))
      << "how'd we get the same image after a cache clear?";
}

}  // namespace
