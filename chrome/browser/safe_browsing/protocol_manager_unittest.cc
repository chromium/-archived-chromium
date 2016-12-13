// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#include "base/logging.h"
#include "base/time.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "chrome/browser/safe_browsing/protocol_manager.h"

using base::Time;
using base::TimeDelta;

class SafeBrowsingProtocolManagerTest : public testing::Test {
};

// Ensure that we respect section 5 of the SafeBrowsing protocol specification.
TEST_F(SafeBrowsingProtocolManagerTest, TestBackOffTimes) {
  SafeBrowsingProtocolManager pm(NULL, NULL, "", "");
  pm.next_update_sec_ = 1800;
  DCHECK(pm.back_off_fuzz_ >= 0.0 && pm.back_off_fuzz_ <= 1.0);

  // No errors received so far.
  EXPECT_EQ(pm.GetNextUpdateTime(false), 1800 * 1000);

  // 1 error.
  EXPECT_EQ(pm.GetNextUpdateTime(true), 60 * 1000);

  // 2 errors.
  int next_time = pm.GetNextUpdateTime(true) / (60 * 1000);  // Minutes
  EXPECT_TRUE(next_time >= 30 && next_time <= 60);

  // 3 errors.
  next_time = pm.GetNextUpdateTime(true) / (60 * 1000);
  EXPECT_TRUE(next_time >= 60 && next_time <= 120);

  // 4 errors.
  next_time = pm.GetNextUpdateTime(true) / (60 * 1000);
  EXPECT_TRUE(next_time >= 120 && next_time <= 240);

  // 5 errors.
  next_time = pm.GetNextUpdateTime(true) / (60 * 1000);
  EXPECT_TRUE(next_time >= 240 && next_time <= 480);

  // 6 errors, reached max backoff.
  EXPECT_EQ(pm.GetNextUpdateTime(true), 480 * 60 * 1000);

  // 7 errors.
  EXPECT_EQ(pm.GetNextUpdateTime(true), 480 * 60 * 1000);

  // Received a successful response.
  EXPECT_EQ(pm.GetNextUpdateTime(false), 1800 * 1000);
}

// Test string combinations with and without MAC.
TEST_F(SafeBrowsingProtocolManagerTest, TestChunkStrings) {
  SafeBrowsingProtocolManager pm(NULL, NULL, "", "");

  // Add and Sub chunks.
  SBListChunkRanges phish("goog-phish-shavar");
  phish.adds = "1,4,6,8-20,99";
  phish.subs = "16,32,64-96";
  EXPECT_EQ(pm.FormatList(phish, false),
            "goog-phish-shavar;a:1,4,6,8-20,99:s:16,32,64-96\n");
  EXPECT_EQ(pm.FormatList(phish, true),
            "goog-phish-shavar;a:1,4,6,8-20,99:s:16,32,64-96:mac\n");

  // Add chunks only.
  phish.subs = "";
  EXPECT_EQ(pm.FormatList(phish, false),
            "goog-phish-shavar;a:1,4,6,8-20,99\n");
  EXPECT_EQ(pm.FormatList(phish, true),
            "goog-phish-shavar;a:1,4,6,8-20,99:mac\n");

  // Sub chunks only.
  phish.adds = "";
  phish.subs = "16,32,64-96";
  EXPECT_EQ(pm.FormatList(phish, false), "goog-phish-shavar;s:16,32,64-96\n");
  EXPECT_EQ(pm.FormatList(phish, true),
            "goog-phish-shavar;s:16,32,64-96:mac\n");

  // No chunks of either type.
  phish.adds = "";
  phish.subs = "";
  EXPECT_EQ(pm.FormatList(phish, false), "goog-phish-shavar;\n");
  EXPECT_EQ(pm.FormatList(phish, true), "goog-phish-shavar;mac\n");
}

// Flakey, see http://code.google.com/p/chromium/issues/detail?id=1880
TEST_F(SafeBrowsingProtocolManagerTest, DISABLED_TestGetHashBackOffTimes) {
  SafeBrowsingProtocolManager pm(NULL, NULL, "", "");

  // No errors or back off time yet.
  EXPECT_EQ(pm.gethash_error_count_, 0);
  EXPECT_TRUE(pm.next_gethash_time_.is_null());

  Time now = Time::Now();

  // 1 error.
  pm.HandleGetHashError();
  EXPECT_EQ(pm.gethash_error_count_, 1);
  TimeDelta margin = TimeDelta::FromSeconds(5);  // Fudge factor.
  Time future = now + TimeDelta::FromMinutes(1);
  EXPECT_TRUE(pm.next_gethash_time_ >= future - margin &&
              pm.next_gethash_time_ <= future + margin);

  // 2 errors.
  pm.HandleGetHashError();
  EXPECT_EQ(pm.gethash_error_count_, 2);
  EXPECT_TRUE(pm.next_gethash_time_ >= now + TimeDelta::FromMinutes(30));
  EXPECT_TRUE(pm.next_gethash_time_ <= now + TimeDelta::FromMinutes(60));

  // 3 errors.
  pm.HandleGetHashError();
  EXPECT_EQ(pm.gethash_error_count_, 3);
  EXPECT_TRUE(pm.next_gethash_time_ >= now + TimeDelta::FromMinutes(60));
  EXPECT_TRUE(pm.next_gethash_time_ <= now + TimeDelta::FromMinutes(120));

  // 4 errors.
  pm.HandleGetHashError();
  EXPECT_EQ(pm.gethash_error_count_, 4);
  EXPECT_TRUE(pm.next_gethash_time_ >= now + TimeDelta::FromMinutes(120));
  EXPECT_TRUE(pm.next_gethash_time_ <= now + TimeDelta::FromMinutes(240));

  // 5 errors.
  pm.HandleGetHashError();
  EXPECT_EQ(pm.gethash_error_count_, 5);
  EXPECT_TRUE(pm.next_gethash_time_ >= now + TimeDelta::FromMinutes(240));
  EXPECT_TRUE(pm.next_gethash_time_ <= now + TimeDelta::FromMinutes(480));

  // 6 errors, reached max backoff.
  pm.HandleGetHashError();
  EXPECT_EQ(pm.gethash_error_count_, 6);
  EXPECT_TRUE(pm.next_gethash_time_ == now + TimeDelta::FromMinutes(480));

  // 7 errors.
  pm.HandleGetHashError();
  EXPECT_EQ(pm.gethash_error_count_, 7);
  EXPECT_TRUE(pm.next_gethash_time_== now + TimeDelta::FromMinutes(480));
}
