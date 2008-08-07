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

#include "base/sys_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"

// Apparently Windows doesn't have constants for these.
static const int kCpLatin1 = 850;
static const int kCpBig5 = 950;

TEST(SysStringsWin, SysWideToUTF8) {
  using base::SysWideToUTF8;
  EXPECT_EQ("Hello, world", SysWideToUTF8(L"Hello, world"));
  EXPECT_EQ("\xe4\xbd\xa0\xe5\xa5\xbd", SysWideToUTF8(L"\x4f60\x597d"));
  EXPECT_EQ("\xF0\x90\x8C\x80", SysWideToUTF8(L"\xd800\xdf00"));  // >16 bits

  // Error case. When Windows finds a UTF-16 character going off the end of
  // a string, it just converts that literal value to UTF-8, even though this
  // is invalid.
  EXPECT_EQ("\xE4\xBD\xA0\xED\xA0\x80zyxw", SysWideToUTF8(L"\x4f60\xd800zyxw"));

  // Test embedded NULLs.
  std::wstring wide_null(L"a");
  wide_null.push_back(0);
  wide_null.push_back('b');

  std::string expected_null("a");
  expected_null.push_back(0);
  expected_null.push_back('b');

  EXPECT_EQ(expected_null, SysWideToUTF8(wide_null));
}

TEST(SysStringsWin, SysUTF8ToWide) {
  using base::SysUTF8ToWide;
  EXPECT_EQ(L"Hello, world", SysUTF8ToWide("Hello, world"));
  EXPECT_EQ(L"\x4f60\x597d", SysUTF8ToWide("\xe4\xbd\xa0\xe5\xa5\xbd"));
  EXPECT_EQ(L"\xd800\xdf00", SysUTF8ToWide("\xF0\x90\x8C\x80"));  // >16 bits

  // Error case. When Windows finds an invalid UTF-8 character, it just skips
  // it. This seems weird because it's inconsistent with the reverse conversion.
  EXPECT_EQ(L"\x4f60zyxw", SysUTF8ToWide("\xe4\xbd\xa0\xe5\xa5zyxw"));

  // Test embedded NULLs.
  std::string utf8_null("a");
  utf8_null.push_back(0);
  utf8_null.push_back('b');

  std::wstring expected_null(L"a");
  expected_null.push_back(0);
  expected_null.push_back('b');

  EXPECT_EQ(expected_null, SysUTF8ToWide(utf8_null));
}
