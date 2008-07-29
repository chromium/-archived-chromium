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

#include "chrome/browser/google_url_tracker.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(GoogleURLTrackerTest, CheckAndConvertURL) {
  static const struct {
    const char* const source_url;
    const bool can_convert;
    const char* const base_url;
  } data[] = {
    { "http://www.google.com/", true, "http://www.google.com/", },
    { "http://google.fr/", true, "http://google.fr/", },
    { "http://google.co.uk/", true, "http://google.co.uk/", },
    { "http://www.google.com.by/", true, "http://www.google.com.by/", },
    { "http://www.google.com/ig", true, "http://www.google.com/", },
    { "http://www.google.com/intl/xx", true, "http://www.google.com/intl/xx", },
    { "http://a.b.c.google.com/", true, "http://a.b.c.google.com/", },
    { "http://www.yahoo.com/", false, "", },
    { "http://google.evil.com/", false, "", },
    { "http://google.com.com.com/", false, "", },
    { "http://google/", false, "", },
  };

  for (size_t i = 0; i < arraysize(data); ++i) {
    GURL base_url;
    const bool can_convert = GoogleURLTracker::CheckAndConvertToGoogleBaseURL(
        GURL(data[i].source_url), &base_url);
    EXPECT_EQ(data[i].can_convert, can_convert);
    EXPECT_STREQ(data[i].base_url, base_url.spec().c_str());
  }
}
