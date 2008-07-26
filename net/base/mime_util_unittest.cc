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

  for (size_t i = 0; i < arraysize(tests); ++i) {
    rv = mime_util::GetMimeTypeFromExtension(tests[i].extension, &mime_type);
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
    { L"c:\\blah.", "", false },
  };

  std::string mime_type;
  bool rv;

  for (size_t i = 0; i < arraysize(tests); ++i) {
    rv = mime_util::GetMimeTypeFromFile(tests[i].file_path, &mime_type);
    EXPECT_EQ(rv, tests[i].valid);
    if (rv)
      EXPECT_EQ(mime_type, tests[i].mime_type);
  }
}

TEST(MimeUtilTest, LookupTypes) {
  EXPECT_EQ(true, mime_util::IsSupportedImageMimeType("image/jpeg"));
  EXPECT_EQ(false, mime_util::IsSupportedImageMimeType("image/lolcat"));
  EXPECT_EQ(true, mime_util::IsSupportedNonImageMimeType("text/html"));
  EXPECT_EQ(false, mime_util::IsSupportedNonImageMimeType("text/virus"));
}

TEST(MimeUtilTest, MatchesMimeType) {
  EXPECT_EQ(true, mime_util::MatchesMimeType("*", "video/x-mpeg"));
  EXPECT_EQ(true, mime_util::MatchesMimeType("video/*", "video/x-mpeg"));
  EXPECT_EQ(true, mime_util::MatchesMimeType("video/x-mpeg", "video/x-mpeg"));
  EXPECT_EQ(true, mime_util::MatchesMimeType("application/*+xml",
                                             "application/html+xml"));
  EXPECT_EQ(true, mime_util::MatchesMimeType("application/*+xml",
                                             "application/+xml"));
  EXPECT_EQ(true, mime_util::MatchesMimeType("aaa*aaa",
                                             "aaaaaa"));
  EXPECT_EQ(false, mime_util::MatchesMimeType("video/", "video/x-mpeg"));
  EXPECT_EQ(false, mime_util::MatchesMimeType("", "video/x-mpeg"));
  EXPECT_EQ(false, mime_util::MatchesMimeType("", ""));
  EXPECT_EQ(false, mime_util::MatchesMimeType("video/x-mpeg", ""));
  EXPECT_EQ(false, mime_util::MatchesMimeType("application/*+xml",
                                              "application/xml"));
  EXPECT_EQ(false, mime_util::MatchesMimeType("application/*+xml",
                                              "application/html+xmlz"));
  EXPECT_EQ(false, mime_util::MatchesMimeType("application/*+xml",
                                              "applcation/html+xml"));
  EXPECT_EQ(false, mime_util::MatchesMimeType("aaa*aaa",
                                              "aaaaa"));
}
