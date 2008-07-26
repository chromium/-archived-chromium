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

#include "base/basictypes.h"
#include "googleurl/src/url_util.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(URLUtil, Scheme) {
  enum SchemeType {
    NO_SCHEME       = 0,
    SCHEME          = 1 << 0,
    STANDARD_SCHEME = 1 << 1,
  };
  const struct {
    const wchar_t* url;
    const char* try_scheme;
    const bool scheme_matches;
    const int flags;
  } tests[] = {
    {L"  ",                 "hello", false, NO_SCHEME},
    {L"foo",                NULL, false, NO_SCHEME},
    {L"google.com/foo:bar", NULL, false, NO_SCHEME},
    {L"Garbage:foo.com",    "garbage", true, SCHEME},
    {L"Garbage:foo.com",    "trash", false, SCHEME},
    {L"gopher:",            "gopher", true, SCHEME | STANDARD_SCHEME},
    {L"About:blank",        "about", true, SCHEME},
    {L"http://foo.com:123", "foo", false, SCHEME | STANDARD_SCHEME},
    {L"file://c/",          "file", true, SCHEME | STANDARD_SCHEME},
  };

  for (size_t i = 0; i < arraysize(tests); i++) {
    int url_len = static_cast<int>(wcslen(tests[i].url));

    url_parse::Component parsed_scheme;
    bool has_scheme = url_parse::ExtractScheme(tests[i].url, url_len,
                                               &parsed_scheme);

    EXPECT_EQ(!!(tests[i].flags & STANDARD_SCHEME),
              url_util::IsStandard(tests[i].url, url_len));
    EXPECT_EQ(!!(tests[i].flags & STANDARD_SCHEME),
              url_util::IsStandardScheme(tests[i].url, parsed_scheme.len));

    url_parse::Component found_scheme;
    EXPECT_EQ(tests[i].scheme_matches,
              url_util::FindAndCompareScheme(tests[i].url, url_len,
                                             tests[i].try_scheme,
                                             &found_scheme));
    EXPECT_EQ(parsed_scheme.begin, found_scheme.begin);
    EXPECT_EQ(parsed_scheme.len, found_scheme.len);
  }
}
