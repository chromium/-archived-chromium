// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/metrics_service.h"

#include <string>
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_POSIX) && defined(LINUX2)
TEST(MetricsServiceTest, ClientIdGeneratesAllZeroes) {
  uint64 bytes[] = { 0, 0 };
  std::string clientid = MetricsService::RandomBytesToGUIDString(bytes);
  EXPECT_EQ("00000000-0000-0000-0000-000000000000", clientid);
}
TEST(MetricsServiceTest, ClientIdGeneratesCorrectly) {
  uint64 bytes[] = { 0x0123456789ABCDEFULL, 0xFEDCBA9876543210ULL };
  std::string clientid = MetricsService::RandomBytesToGUIDString(bytes);
  EXPECT_EQ("01234567-89AB-CDEF-FEDC-BA9876543210", clientid);
}

TEST(MetricsServiceTest, ClientIdCorrectlyFormatted) {
  std::string clientid = MetricsService::GenerateClientID();
  EXPECT_EQ(36U, clientid.length());
  std::string hexchars = "0123456789ABCDEF";
  for (uint32 i = 0; i < clientid.length(); i++) {
    char current = clientid.at(i);
    if (i == 8 || i == 13 || i == 18 || i == 23) {
      EXPECT_EQ('-', current);
    } else {
      EXPECT_TRUE(std::string::npos != hexchars.find(current));
    }
  }
}
#endif
