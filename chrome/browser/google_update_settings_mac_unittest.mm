// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <Foundation/Foundation.h>

#include "base/basictypes.h"
#include "base/logging.h"
#include "base/sys_string_conversions.h"
#include "chrome/installer/util/google_update_constants.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

namespace google_update {

bool GetCollectStatsConsentFromDictionary(NSDictionary* dict);

}  // namespace google_update


class GoogleUpdateTest : public PlatformTest {
};

TEST_F(GoogleUpdateTest, StatsConstent) {
  using google_update::GetCollectStatsConsentFromDictionary;

  // Stats are off by default.
  NSDictionary* empty_dict = [NSDictionary dictionary];
  ASSERT_FALSE(GetCollectStatsConsentFromDictionary(empty_dict));

  NSString* collect_stats_key = base::SysWideToNSString(
      google_update::kRegUsageStatsField);

  // Stats reporting is ON.
  NSNumber* stats_enabled = [NSNumber numberWithBool:YES];
  NSDictionary* enabled_dict = [NSDictionary
      dictionaryWithObject:stats_enabled
                    forKey:collect_stats_key];
  ASSERT_TRUE(GetCollectStatsConsentFromDictionary(enabled_dict));

  // Stats reporting is OFF.
  NSNumber* stats_disabled = [NSNumber numberWithBool:NO];
  NSDictionary* disabled_dict = [NSDictionary
      dictionaryWithObject:stats_disabled
                    forKey:collect_stats_key];
  ASSERT_FALSE(GetCollectStatsConsentFromDictionary(disabled_dict));

  // Check that we fail gracefully if an object of the wrong type is present.
  NSDictionary* wrong_type_dict = [NSDictionary
      dictionaryWithObject:empty_dict
                    forKey:collect_stats_key];
  ASSERT_FALSE(GetCollectStatsConsentFromDictionary(wrong_type_dict));
}
