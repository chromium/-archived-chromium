// Copyright 2008, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//

#include "base/logging.h"
#include "base/time.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "chrome/browser/safe_browsing/protocol_manager.h"


// Ensure that we respect section 5 of the SafeBrowsing protocol specification.
TEST(SafeBrowsingProtocolManagerTest, TestBackOffTimes) {
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
TEST(SafeBrowsingProtocolManagerTest, TestChunkStrings) {
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
  EXPECT_EQ(pm.FormatList(phish, true), "goog-phish-shavar;s:16,32,64-96:mac\n");

  // No chunks of either type.
  phish.adds = "";
  phish.subs = "";
  EXPECT_EQ(pm.FormatList(phish, false), "goog-phish-shavar;\n");
  EXPECT_EQ(pm.FormatList(phish, true), "goog-phish-shavar;mac\n");
}

TEST(SafeBrowsingProtocolManagerTest, TestGetHashBackOffTimes) {
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
