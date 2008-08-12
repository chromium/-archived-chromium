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

#include "chrome/browser/printing/units.h"
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
  EXPECT_EQ(4000000000, ConvertUnit(2, 1, 2000000000));

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
