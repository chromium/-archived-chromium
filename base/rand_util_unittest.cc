// Copyright (c) 2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/rand_util.h"

#include <limits>

#include "testing/gtest/include/gtest/gtest.h"

namespace {

const int kIntMin = std::numeric_limits<int>::min();
const int kIntMax = std::numeric_limits<int>::max();

}  // namespace

TEST(RandUtilTest, SameMinAndMax) {
  EXPECT_EQ(base::RandInt(0, 0), 0);
  EXPECT_EQ(base::RandInt(kIntMin, kIntMin), kIntMin);
  EXPECT_EQ(base::RandInt(kIntMax, kIntMax), kIntMax);
}
