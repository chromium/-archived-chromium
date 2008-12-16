// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(data); ++i) {
    GURL base_url;
    const bool can_convert = GoogleURLTracker::CheckAndConvertToGoogleBaseURL(
        GURL(data[i].source_url), &base_url);
    EXPECT_EQ(data[i].can_convert, can_convert);
    EXPECT_STREQ(data[i].base_url, base_url.spec().c_str());
  }
}

