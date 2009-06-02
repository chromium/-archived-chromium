// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/force_tls_state.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class ForceTLSStateTest : public testing::Test {
};

TEST_F(ForceTLSStateTest, BogusHeaders) {
  int max_age = 42;
  bool include_subdomains = false;

  EXPECT_FALSE(net::ForceTLSState::ParseHeader(
      "", &max_age, &include_subdomains));
  EXPECT_FALSE(net::ForceTLSState::ParseHeader(
      "    ", &max_age, &include_subdomains));
  EXPECT_FALSE(net::ForceTLSState::ParseHeader(
      "abc", &max_age, &include_subdomains));
  EXPECT_FALSE(net::ForceTLSState::ParseHeader(
      "  abc", &max_age, &include_subdomains));
  EXPECT_FALSE(net::ForceTLSState::ParseHeader(
      "  abc   ", &max_age, &include_subdomains));
  EXPECT_FALSE(net::ForceTLSState::ParseHeader(
      "max-age", &max_age, &include_subdomains));
  EXPECT_FALSE(net::ForceTLSState::ParseHeader(
      "  max-age", &max_age, &include_subdomains));
  EXPECT_FALSE(net::ForceTLSState::ParseHeader(
      "  max-age  ", &max_age, &include_subdomains));
  EXPECT_FALSE(net::ForceTLSState::ParseHeader(
      "max-age=", &max_age, &include_subdomains));
  EXPECT_FALSE(net::ForceTLSState::ParseHeader(
      "   max-age=", &max_age, &include_subdomains));
  EXPECT_FALSE(net::ForceTLSState::ParseHeader(
      "   max-age  =", &max_age, &include_subdomains));
  EXPECT_FALSE(net::ForceTLSState::ParseHeader(
      "   max-age=   ", &max_age, &include_subdomains));
  EXPECT_FALSE(net::ForceTLSState::ParseHeader(
      "   max-age  =     ", &max_age, &include_subdomains));
  EXPECT_FALSE(net::ForceTLSState::ParseHeader(
      "   max-age  =     xy", &max_age, &include_subdomains));
  EXPECT_FALSE(net::ForceTLSState::ParseHeader(
      "   max-age  =     3488a923", &max_age, &include_subdomains));
  EXPECT_FALSE(net::ForceTLSState::ParseHeader(
      "max-age=3488a923  ", &max_age, &include_subdomains));
  EXPECT_FALSE(net::ForceTLSState::ParseHeader(
      "max-ag=3488923", &max_age, &include_subdomains));
  EXPECT_FALSE(net::ForceTLSState::ParseHeader(
      "max-aged=3488923", &max_age, &include_subdomains));
  EXPECT_FALSE(net::ForceTLSState::ParseHeader(
      "max-age==3488923", &max_age, &include_subdomains));
  EXPECT_FALSE(net::ForceTLSState::ParseHeader(
      "amax-age=3488923", &max_age, &include_subdomains));
  EXPECT_FALSE(net::ForceTLSState::ParseHeader(
      "max-age=-3488923", &max_age, &include_subdomains));
  EXPECT_FALSE(net::ForceTLSState::ParseHeader(
      "max-age=3488923;", &max_age, &include_subdomains));
  EXPECT_FALSE(net::ForceTLSState::ParseHeader(
      "max-age=3488923     e", &max_age, &include_subdomains));
  EXPECT_FALSE(net::ForceTLSState::ParseHeader(
      "max-age=3488923     includesubdomain", &max_age, &include_subdomains));
  EXPECT_FALSE(net::ForceTLSState::ParseHeader(
      "max-age=3488923includesubdomains", &max_age, &include_subdomains));
  EXPECT_FALSE(net::ForceTLSState::ParseHeader(
      "max-age=3488923=includesubdomains", &max_age, &include_subdomains));
  EXPECT_FALSE(net::ForceTLSState::ParseHeader(
      "max-age=3488923 includesubdomainx", &max_age, &include_subdomains));
  EXPECT_FALSE(net::ForceTLSState::ParseHeader(
      "max-age=3488923 includesubdomain=", &max_age, &include_subdomains));
  EXPECT_FALSE(net::ForceTLSState::ParseHeader(
      "max-age=3488923 includesubdomain=true", &max_age, &include_subdomains));
  EXPECT_FALSE(net::ForceTLSState::ParseHeader(
      "max-age=3488923 includesubdomainsx", &max_age, &include_subdomains));
  EXPECT_FALSE(net::ForceTLSState::ParseHeader(
      "max-age=3488923 includesubdomains x", &max_age, &include_subdomains));
  EXPECT_FALSE(net::ForceTLSState::ParseHeader(
      "max-age=34889.23 includesubdomains", &max_age, &include_subdomains));

  EXPECT_EQ(max_age, 42);
  EXPECT_FALSE(include_subdomains);
}

TEST_F(ForceTLSStateTest, ValidHeaders) {
  int max_age = 42;
  bool include_subdomains = true;

  EXPECT_TRUE(net::ForceTLSState::ParseHeader(
      "max-age=243", &max_age, &include_subdomains));
  EXPECT_EQ(max_age, 243);
  EXPECT_FALSE(include_subdomains);

  EXPECT_TRUE(net::ForceTLSState::ParseHeader(
      "  Max-agE    = 567", &max_age, &include_subdomains));
  EXPECT_EQ(max_age, 567);
  EXPECT_FALSE(include_subdomains);

  EXPECT_TRUE(net::ForceTLSState::ParseHeader(
      "  mAx-aGe    = 890      ", &max_age, &include_subdomains));
  EXPECT_EQ(max_age, 890);
  EXPECT_FALSE(include_subdomains);

  EXPECT_TRUE(net::ForceTLSState::ParseHeader(
      "max-age=123 incLudesUbdOmains", &max_age, &include_subdomains));
  EXPECT_EQ(max_age, 123);
  EXPECT_TRUE(include_subdomains);

  EXPECT_TRUE(net::ForceTLSState::ParseHeader(
      "max-age=394082038    incLudesUbdOmains", &max_age, &include_subdomains));
  EXPECT_EQ(max_age, 394082038);
  EXPECT_TRUE(include_subdomains);

  EXPECT_TRUE(net::ForceTLSState::ParseHeader(
      "  max-age=0    incLudesUbdOmains   ", &max_age, &include_subdomains));
  EXPECT_EQ(max_age, 0);
  EXPECT_TRUE(include_subdomains);
}

}  // namespace
