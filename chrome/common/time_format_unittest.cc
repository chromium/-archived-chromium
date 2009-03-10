// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <time.h>

#include "base/basictypes.h"
#include "base/time.h"
#include "chrome/common/l10n_util.h"
#include "chrome/common/time_format.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::Time;
using base::TimeDelta;

TEST(TimeFormat, RelativeDate) {
  Time now = Time::Now();
  std::wstring today_str = TimeFormat::RelativeDate(now, NULL);
  EXPECT_EQ(L"Today", today_str);

  Time yesterday = now - TimeDelta::FromDays(1);
  std::wstring yesterday_str = TimeFormat::RelativeDate(yesterday, NULL);
  EXPECT_EQ(L"Yesterday", yesterday_str);

  Time two_days_ago = now - TimeDelta::FromDays(2);
  std::wstring two_days_ago_str = TimeFormat::RelativeDate(two_days_ago, NULL);
  EXPECT_TRUE(two_days_ago_str.empty());

  Time a_week_ago = now - TimeDelta::FromDays(7);
  std::wstring a_week_ago_str = TimeFormat::RelativeDate(a_week_ago, NULL);
  EXPECT_TRUE(a_week_ago_str.empty());
}

namespace {
void TestRemainingTime(const TimeDelta delta,
                       const std::wstring& expected_short,
                       const std::wstring& expected_long) {
  EXPECT_EQ(expected_short, TimeFormat::TimeRemainingShort(delta));
  EXPECT_EQ(expected_long, TimeFormat::TimeRemaining(delta));
}

} // namespace

TEST(TimeFormat, RemainingTime) {
  const TimeDelta one_day = TimeDelta::FromDays(1);
  const TimeDelta three_days = TimeDelta::FromDays(3);
  const TimeDelta one_hour = TimeDelta::FromHours(1);
  const TimeDelta four_hours = TimeDelta::FromHours(4);
  const TimeDelta one_min = TimeDelta::FromMinutes(1);
  const TimeDelta three_mins = TimeDelta::FromMinutes(3);
  const TimeDelta one_sec = TimeDelta::FromSeconds(1);
  const TimeDelta five_secs = TimeDelta::FromSeconds(5);
  const TimeDelta twohundred_millisecs = TimeDelta::FromMilliseconds(200);

  // TODO(jungshik) : These test only pass when the OS locale is 'en'.
  // We need to add SetUp() and TearDown() to set the locale to 'en'.
  TestRemainingTime(twohundred_millisecs, L"0 secs", L"0 secs left");
  TestRemainingTime(one_sec - twohundred_millisecs, L"0 secs", L"0 secs left");
  TestRemainingTime(one_sec + twohundred_millisecs, L"1 sec", L"1 sec left");
  TestRemainingTime(five_secs + twohundred_millisecs, L"5 secs",
                    L"5 secs left");
  TestRemainingTime(one_min + five_secs, L"1 min", L"1 min left");
  TestRemainingTime(three_mins + twohundred_millisecs,
                    L"3 mins", L"3 mins left");
  TestRemainingTime(one_hour + five_secs, L"1 hour", L"1 hour left");
  TestRemainingTime(four_hours + five_secs, L"4 hours", L"4 hours left");
  TestRemainingTime(one_day + five_secs, L"1 day", L"1 day left");
  TestRemainingTime(three_days, L"3 days", L"3 days left");
  TestRemainingTime(three_days + four_hours, L"3 days", L"3 days left");
}
