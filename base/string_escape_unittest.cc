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

#include "testing/gtest/include/gtest/gtest.h"
#include "base/string_escape.h"

TEST(StringEscapeTest, JavascriptDoubleQuote) {
  static const char* kToEscape          = "\b\001aZ\"\\wee";
  static const char* kEscaped           = "\\b\\x01aZ\\\"\\\\wee";
  static const wchar_t* kUToEscape      = L"\b\x0001" L"a\x123fZ\"\\wee";
  static const char* kUEscaped          = "\\b\\x01a\\u123FZ\\\"\\\\wee";
  static const char* kUEscapedQuoted    = "\"\\b\\x01a\\u123FZ\\\"\\\\wee\"";

  std::string out;

  // Test wide unicode escaping
  out = "testy: ";
  string_escape::JavascriptDoubleQuote(std::wstring(kUToEscape), false, &out);
  ASSERT_EQ(std::string("testy: ") + kUEscaped, out);

  out = "testy: ";
  string_escape::JavascriptDoubleQuote(std::wstring(kUToEscape), true, &out);
  ASSERT_EQ(std::string("testy: ") + kUEscapedQuoted, out);

  // Test null and high bit / negative unicode values
  std::wstring wstr(L"TeSt");
  wstr.push_back(0);
  wstr.push_back(0xffb1);
  wstr.push_back(0x00ff);

  out = "testy: ";
  string_escape::JavascriptDoubleQuote(wstr, false, &out);
  ASSERT_EQ("testy: TeSt\\x00\\uFFB1\\xFF", out);

  // Test escaping of 7bit ascii
  out = "testy: ";
  string_escape::JavascriptDoubleQuote(std::string(kToEscape), false, &out);
  ASSERT_EQ(std::string("testy: ") + kEscaped, out);

  // Test null, non-printable, and non-7bit
  std::string str("TeSt");
  str.push_back(0);
  str.push_back(15);
  str.push_back(127);
  str.push_back(-16);
  str.push_back(-128);
  str.push_back('!');

  out = "testy: ";
  string_escape::JavascriptDoubleQuote(str, false, &out);
  ASSERT_EQ("testy: TeSt\\x00\\x0F\\x7F\xf0\x80!", out);
}
