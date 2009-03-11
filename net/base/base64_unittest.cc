// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/base64.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class Base64Test : public testing::Test {
};

}  // namespace

TEST(Base64Test, Basic) {
  const std::string kText = "hello world";
  const std::string kBase64Text = "aGVsbG8gd29ybGQ=";

  std::string encoded, decoded;
  bool ok;

  ok = net::Base64Encode(kText, &encoded);
  EXPECT_TRUE(ok);
  EXPECT_EQ(kBase64Text, encoded);

  ok = net::Base64Decode(encoded, &decoded);
  EXPECT_TRUE(ok);
  EXPECT_EQ(kText, decoded);
}
