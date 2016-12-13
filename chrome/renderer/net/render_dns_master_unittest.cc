// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Single threaded tests of RenderDnsMaster functionality.

#include "chrome/renderer/net/render_dns_master.h"
#include "testing/gtest/include/gtest/gtest.h"

#include <algorithm>

namespace {

class RenderDnsMasterTest : public testing::Test {
};

TEST(RenderDnsMasterTest, NumericIpDiscardCheck) {
  // Regular names.
  const std::string A("a.com"), B("b.net"), C("www.other.uk");
  // Combination of digits plus dots.
  const std::string N1("1.3."), N2("5.5.7.12");

#define TESTNAME(string) RenderDnsMaster::is_numeric_ip((string.data()), \
                                                        (string).size())

  EXPECT_TRUE(TESTNAME(N1));
  EXPECT_TRUE(TESTNAME(N2));

  EXPECT_FALSE(TESTNAME(A));
  EXPECT_FALSE(TESTNAME(B));
  EXPECT_FALSE(TESTNAME(C));

#undef TESTNAME
}

}  // namespace anonymous
