// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"

#include "chrome/test/testing_profile.h"
#include "chrome/browser/extensions/extension_process_manager.h"
#include "chrome/browser/tab_contents/site_instance.h"

class ExtensionProcessManagerTest : public testing::Test {
};

// Test that extensions get grouped in the right SiteInstance (and therefore
// process) based on their URLs.
TEST(ExtensionProcessManagerTest, ProcessGrouping) {
  ExtensionProcessManager* manager = ExtensionProcessManager::GetInstance();

  // Extensions in different profiles should always be different SiteInstances.
  TestingProfile profile1(1);
  TestingProfile profile2(2);

  // Extensions with common origins ("scheme://id/") should be grouped in the
  // same SiteInstance.
  GURL ext1_url1("chrome-extensions://ext1_id/index.html");
  GURL ext1_url2("chrome-extensions://ext1_id/toolstrips/toolstrip.html");
  GURL ext2_url1("chrome-extensions://ext2_id/index.html");

  scoped_refptr<SiteInstance> site11 =
      manager->GetSiteInstanceForURL(ext1_url1, &profile1);
  scoped_refptr<SiteInstance> site12 =
      manager->GetSiteInstanceForURL(ext1_url2, &profile1);
  EXPECT_EQ(site11, site12);

  scoped_refptr<SiteInstance> site21 =
      manager->GetSiteInstanceForURL(ext2_url1, &profile1);
  EXPECT_NE(site11, site21);

  scoped_refptr<SiteInstance> other_profile_site =
      manager->GetSiteInstanceForURL(ext1_url1, &profile2);
  EXPECT_NE(site11, other_profile_site);
}
