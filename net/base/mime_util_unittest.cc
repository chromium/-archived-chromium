// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "net/base/mime_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
  class MimeUtilTest : public testing::Test {
  };
}

TEST(MimeUtilTest, ExtensionTest) {
  const struct {
    const wchar_t* extension;
    const char* mime_type;
    bool valid;
  } tests[] = {
    { L"png", "image/png", true },
    { L"css", "text/css", true },
    { L"pjp", "image/jpeg", true },
    { L"pjpeg", "image/jpeg", true },
    { L"not an extension / for sure", "", false },
  };

  std::string mime_type;
  bool rv;

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(tests); ++i) {
    rv = net::GetMimeTypeFromExtension(tests[i].extension, &mime_type);
    EXPECT_EQ(rv, tests[i].valid);
    if (rv)
      EXPECT_EQ(mime_type, tests[i].mime_type);
  }
}

TEST(MimeUtilTest, FileTest) {
  const struct {
    const wchar_t* file_path;
    const char* mime_type;
    bool valid;
  } tests[] = {
    { L"c:\\foo\\bar.css", "text/css", true },
    { L"c:\\blah", "", false },
    { L"/usr/local/bin/mplayer", "", false },
    { L"/home/foo/bar.css", "text/css", true },
    { L"/blah.", "", false },
    { L"c:\\blah.", "", false },
  };

  std::string mime_type;
  bool rv;

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(tests); ++i) {
    rv = net::GetMimeTypeFromFile(tests[i].file_path, &mime_type);
    EXPECT_EQ(rv, tests[i].valid);
    if (rv)
      EXPECT_EQ(mime_type, tests[i].mime_type);
  }
}

TEST(MimeUtilTest, LookupTypes) {
  EXPECT_EQ(true, net::IsSupportedImageMimeType("image/jpeg"));
  EXPECT_EQ(false, net::IsSupportedImageMimeType("image/lolcat"));
  EXPECT_EQ(true, net::IsSupportedNonImageMimeType("text/html"));
  EXPECT_EQ(false, net::IsSupportedNonImageMimeType("text/virus"));
}

TEST(MimeUtilTest, MatchesMimeType) {
  EXPECT_EQ(true, net::MatchesMimeType("*", "video/x-mpeg"));
  EXPECT_EQ(true, net::MatchesMimeType("video/*", "video/x-mpeg"));
  EXPECT_EQ(true, net::MatchesMimeType("video/x-mpeg", "video/x-mpeg"));
  EXPECT_EQ(true, net::MatchesMimeType("application/*+xml",
                                             "application/html+xml"));
  EXPECT_EQ(true, net::MatchesMimeType("application/*+xml",
                                             "application/+xml"));
  EXPECT_EQ(true, net::MatchesMimeType("aaa*aaa",
                                             "aaaaaa"));
  EXPECT_EQ(false, net::MatchesMimeType("video/", "video/x-mpeg"));
  EXPECT_EQ(false, net::MatchesMimeType("", "video/x-mpeg"));
  EXPECT_EQ(false, net::MatchesMimeType("", ""));
  EXPECT_EQ(false, net::MatchesMimeType("video/x-mpeg", ""));
  EXPECT_EQ(false, net::MatchesMimeType("application/*+xml",
                                              "application/xml"));
  EXPECT_EQ(false, net::MatchesMimeType("application/*+xml",
                                              "application/html+xmlz"));
  EXPECT_EQ(false, net::MatchesMimeType("application/*+xml",
                                              "applcation/html+xml"));
  EXPECT_EQ(false, net::MatchesMimeType("aaa*aaa",
                                              "aaaaa"));
}

