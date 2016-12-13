// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/units.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest/include/gtest/gtest-spi.h"

using namespace printing;

TEST(UnitsTest, Convertions) {
  EXPECT_EQ(100, ConvertUnit(100, 100, 100));
  EXPECT_EQ(-100, ConvertUnit(-100, 100, 100));
  EXPECT_EQ(0, ConvertUnit(0, 100, 100));
  EXPECT_EQ(99, ConvertUnit(99, 100, 100));
  EXPECT_EQ(101, ConvertUnit(101, 100, 100));
  EXPECT_EQ(99900, ConvertUnit(999, 10, 1000));
  EXPECT_EQ(100100, ConvertUnit(1001, 10, 1000));

  // Rounding.
  EXPECT_EQ(10, ConvertUnit(999, 1000, 10));
  EXPECT_EQ(10, ConvertUnit(950, 1000, 10));
  EXPECT_EQ(9, ConvertUnit(949, 1000, 10));
  EXPECT_EQ(10, ConvertUnit(1001, 1000, 10));
  EXPECT_EQ(10, ConvertUnit(1049, 1000, 10));
  EXPECT_EQ(11, ConvertUnit(1050, 1000, 10));
  EXPECT_EQ(-10, ConvertUnit(-999, 1000, 10));
  EXPECT_EQ(-10, ConvertUnit(-950, 1000, 10));
  EXPECT_EQ(-9, ConvertUnit(-949, 1000, 10));
  EXPECT_EQ(-10, ConvertUnit(-1001, 1000, 10));
  EXPECT_EQ(-10, ConvertUnit(-1049, 1000, 10));
  EXPECT_EQ(-11, ConvertUnit(-1050, 1000, 10));

  EXPECT_EQ(0, ConvertUnit(2, 1000000000, 1));
  EXPECT_EQ(2000000000, ConvertUnit(2, 1, 1000000000));
  EXPECT_EQ(4000000000U,
            static_cast<unsigned int>(ConvertUnit(2, 1, 2000000000)));

  EXPECT_EQ(100, ConvertUnitDouble(100, 100, 100));
  EXPECT_EQ(-100, ConvertUnitDouble(-100, 100, 100));
  EXPECT_EQ(0, ConvertUnitDouble(0, 100, 100));
  EXPECT_EQ(0.000002, ConvertUnitDouble(2, 1000, 0.001));
  EXPECT_EQ(2000000, ConvertUnitDouble(2, 0.001, 1000));

  EXPECT_EQ(kHundrethsMMPerInch, ConvertMilliInchToHundredThousanthMeter(1000));
  EXPECT_EQ(-kHundrethsMMPerInch,
            ConvertMilliInchToHundredThousanthMeter(-1000));
  EXPECT_EQ(0, ConvertMilliInchToHundredThousanthMeter(0));
  EXPECT_EQ(1000, ConvertHundredThousanthMeterToMilliInch(kHundrethsMMPerInch));
  EXPECT_EQ(-1000,
            ConvertHundredThousanthMeterToMilliInch(-kHundrethsMMPerInch));
  EXPECT_EQ(0, ConvertHundredThousanthMeterToMilliInch(0));
}
