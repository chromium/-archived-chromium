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

#include "base/fixed_string.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
  class FixedStringTest : public testing::Test {
  };
}

TEST(FixedStringTest, TestBasic) {
  const wchar_t kData[] = L"hello world";

  FixedString<wchar_t, 40> buf;

  buf.Append(kData);
  EXPECT_EQ(arraysize(kData)-1, buf.size());
  EXPECT_EQ(0, wcscmp(kData, buf.get()));

  buf.Append(' ');
  buf.Append(kData);
  const wchar_t kExpected[] = L"hello world hello world";
  EXPECT_EQ(arraysize(kExpected)-1, buf.size());
  EXPECT_EQ(0, wcscmp(kExpected, buf.get()));
  EXPECT_EQ(false, buf.was_truncated());
}

// Disable this test in debug builds since overflow asserts.
TEST(FixedStringTest, TestOverflow) {
  FixedString<wchar_t, 5> buf;
  buf.Append(L"hello world");
  // The following static_cast is necessary to make Mac gcc happy, so don't
  // remove it unless you've verified that it works there.
  EXPECT_EQ(static_cast<size_t>(0), buf.size());
  EXPECT_EQ(0, buf.get()[0]);
  EXPECT_EQ(true, buf.was_truncated());
}
