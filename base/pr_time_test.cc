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

#include "base/logging.h"
#include "base/third_party/nspr/prtime.h"
#include "base/time.h"
#include "testing/gtest/include/gtest/gtest.h"

#include <time.h>

namespace {

const int64 kMicrosecondsPerSecond = 1000000i64;

// time_t representation of 15th Oct 2007 12:45:00 PDT
PRTime comparison_time_pdt = 1192477500 * kMicrosecondsPerSecond;

// Specialized test fixture allowing time strings without timezones to be
// tested by comparing them to a known time in the local zone.
class PRTimeTest : public testing::Test {
 protected:
  virtual void SetUp() {
    // Use mktime to get a time_t, and turn it into a PRTime by converting
    // seconds to microseconds.  Use 15th Oct 2007 12:45:00 local.  This
    // must be a time guaranteed to be outside of a DST fallback hour in
    // any timezone.
    struct tm local_comparison_tm = {
      0,            // second
      45,           // minute
      12,           // hour
      15,           // day of month
      10 - 1,       // month
      2007 - 1900,  // year
      0,            // day of week (ignored, output only)
      0,            // day of year (ignored, output only)
      -1            // DST in effect, -1 tells mktime to figure it out
    };
    comparison_time_local_ = mktime(&local_comparison_tm) *
                             kMicrosecondsPerSecond;
    ASSERT_GT(comparison_time_local_, 0);
  }

  PRTime comparison_time_local_;
};

}  // namespace

// Tests the PR_ParseTimeString nspr helper function for
// a variety of time strings.
TEST_F(PRTimeTest, ParseTimeTest1) {
  //time_t current_time = 0;
  PRTime current_time = 0;
  time(&current_time);

  tm local_time = {0};
  localtime_s(&local_time,&current_time);

  char time_buf[MAX_PATH] = {0};
  asctime_s(time_buf, arraysize(time_buf), &local_time);
  current_time *= PR_USEC_PER_SEC;

  PRTime parsed_time = 0;
  PRStatus result = PR_ParseTimeString(time_buf, PR_FALSE, &parsed_time);
  EXPECT_EQ(PR_SUCCESS, result);
  EXPECT_EQ(current_time,parsed_time);
}

TEST_F(PRTimeTest, ParseTimeTest2) {
  PRTime parsed_time = 0;
  PRStatus result = PR_ParseTimeString("Mon, 15 Oct 2007 19:45:00 GMT",
                                       PR_FALSE, &parsed_time);
  EXPECT_EQ(PR_SUCCESS, result);
  EXPECT_EQ(parsed_time, comparison_time_pdt);
}

TEST_F(PRTimeTest, ParseTimeTest3) {
  PRTime parsed_time = 0;
  PRStatus result = PR_ParseTimeString("15 Oct 07 12:45:00", PR_FALSE,
                                       &parsed_time);
  EXPECT_EQ(PR_SUCCESS, result);
  EXPECT_EQ(parsed_time, comparison_time_local_);
}

TEST_F(PRTimeTest, ParseTimeTest4) {
  PRTime parsed_time = 0;
  PRStatus result = PR_ParseTimeString("15 Oct 07 19:45 GMT", PR_FALSE,
                                       &parsed_time);
  EXPECT_EQ(PR_SUCCESS, result);
  EXPECT_EQ(parsed_time, comparison_time_pdt);
}

TEST_F(PRTimeTest, ParseTimeTest5) {
  PRTime parsed_time = 0;
  PRStatus result = PR_ParseTimeString("Mon Oct 15 12:45 PDT 2007",
                                       PR_FALSE, &parsed_time);
  EXPECT_EQ(PR_SUCCESS, result);
  EXPECT_EQ(parsed_time, comparison_time_pdt);
}

TEST_F(PRTimeTest, ParseTimeTest6) {
  PRTime parsed_time = 0;
  PRStatus result = PR_ParseTimeString("Monday, Oct 15, 2007 12:45 PM",
                                       PR_FALSE, &parsed_time);
  EXPECT_EQ(PR_SUCCESS, result);
  EXPECT_EQ(parsed_time, comparison_time_local_);
}

TEST_F(PRTimeTest, ParseTimeTest7) {
  PRTime parsed_time = 0;
  PRStatus result = PR_ParseTimeString("10/15/07 12:45:00 PM", PR_FALSE,
                                       &parsed_time);
  EXPECT_EQ(PR_SUCCESS, result);
  EXPECT_EQ(parsed_time, comparison_time_local_);
}

TEST_F(PRTimeTest, ParseTimeTest8) {
  PRTime parsed_time = 0;
  PRStatus result = PR_ParseTimeString("15-OCT-2007 12:45pm", PR_FALSE,
                                       &parsed_time);
  EXPECT_EQ(PR_SUCCESS, result);
  EXPECT_EQ(parsed_time, comparison_time_local_);
}

TEST_F(PRTimeTest, ParseTimeTest9) {
  PRTime parsed_time = 0;
  PRStatus result = PR_ParseTimeString("16 Oct 2007 4:45-JST (Tuesday)",
                                       PR_FALSE, &parsed_time);
  EXPECT_EQ(PR_SUCCESS, result);
  EXPECT_EQ(parsed_time, comparison_time_pdt);
}

// This tests the Time::FromString wrapper over PR_ParseTimeString
TEST_F(PRTimeTest, ParseTimeTest10) {
  Time parsed_time;
  bool result = Time::FromString(L"15/10/07 12:45",&parsed_time);
  EXPECT_EQ(true, result);

  time_t computed_time = parsed_time.ToTimeT();
  time_t time_to_compare = comparison_time_local_ / kMicrosecondsPerSecond;
  EXPECT_EQ(computed_time, time_to_compare);
}

// This tests the Time::FromString wrapper over PR_ParseTimeString
TEST_F(PRTimeTest, ParseTimeTest11) {
  Time parsed_time;
  bool result = Time::FromString(L"Mon, 15 Oct 2007 19:45:00 GMT",
                                 &parsed_time);
  EXPECT_EQ(true, result);

  time_t computed_time = parsed_time.ToTimeT();
  time_t time_to_compare = comparison_time_pdt / kMicrosecondsPerSecond;
  EXPECT_EQ(computed_time, time_to_compare);
}
