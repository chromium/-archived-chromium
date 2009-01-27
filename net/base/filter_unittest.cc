// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/filter.h"
#include "testing/gtest/include/gtest/gtest.h"

class FilterTest : public testing::Test {
};

TEST(FilterTest, ContentTypeId) {
  // Check for basic translation of Content-Encoding, including case variations.
  EXPECT_EQ(Filter::FILTER_TYPE_DEFLATE,
            Filter::ConvertEncodingToType("deflate"));
  EXPECT_EQ(Filter::FILTER_TYPE_DEFLATE,
            Filter::ConvertEncodingToType("deflAte"));
  EXPECT_EQ(Filter::FILTER_TYPE_GZIP,
            Filter::ConvertEncodingToType("gzip"));
  EXPECT_EQ(Filter::FILTER_TYPE_GZIP,
            Filter::ConvertEncodingToType("GzIp"));
  EXPECT_EQ(Filter::FILTER_TYPE_GZIP,
            Filter::ConvertEncodingToType("x-gzip"));
  EXPECT_EQ(Filter::FILTER_TYPE_GZIP,
            Filter::ConvertEncodingToType("X-GzIp"));
  EXPECT_EQ(Filter::FILTER_TYPE_BZIP2,
            Filter::ConvertEncodingToType("bzip2"));
  EXPECT_EQ(Filter::FILTER_TYPE_BZIP2,
            Filter::ConvertEncodingToType("BZiP2"));
  EXPECT_EQ(Filter::FILTER_TYPE_BZIP2,
            Filter::ConvertEncodingToType("x-bzip2"));
  EXPECT_EQ(Filter::FILTER_TYPE_BZIP2,
            Filter::ConvertEncodingToType("X-BZiP2"));
  EXPECT_EQ(Filter::FILTER_TYPE_SDCH,
            Filter::ConvertEncodingToType("sdch"));
  EXPECT_EQ(Filter::FILTER_TYPE_SDCH,
            Filter::ConvertEncodingToType("sDcH"));
  EXPECT_EQ(Filter::FILTER_TYPE_UNSUPPORTED,
            Filter::ConvertEncodingToType("weird"));
  EXPECT_EQ(Filter::FILTER_TYPE_UNSUPPORTED,
            Filter::ConvertEncodingToType("strange"));
}

// Check various fixups that modify content encoding lists.
TEST(FilterTest, ApacheGzip) {
  // Check that redundant gzip mime type removes only solo gzip encoding.
  const bool is_sdch_response = false;
  const std::string kGzipMime1("application/x-gzip");
  const std::string kGzipMime2("application/gzip");
  const std::string kGzipMime3("application/x-gunzip");
  std::vector<Filter::FilterType> encoding_types;

  // First show it removes the gzip, given any gzip style mime type.
  encoding_types.clear();
  encoding_types.push_back(Filter::FILTER_TYPE_GZIP);
  Filter::FixupEncodingTypes(is_sdch_response, kGzipMime1, &encoding_types);
  EXPECT_TRUE(encoding_types.empty());

  encoding_types.clear();
  encoding_types.push_back(Filter::FILTER_TYPE_GZIP);
  Filter::FixupEncodingTypes(is_sdch_response, kGzipMime2, &encoding_types);
  EXPECT_TRUE(encoding_types.empty());

  encoding_types.clear();
  encoding_types.push_back(Filter::FILTER_TYPE_GZIP);
  Filter::FixupEncodingTypes(is_sdch_response, kGzipMime3, &encoding_types);
  EXPECT_TRUE(encoding_types.empty());

  // Check to be sure it doesn't remove everything when it has such a type.
  encoding_types.clear();
  encoding_types.push_back(Filter::FILTER_TYPE_SDCH);
  Filter::FixupEncodingTypes(is_sdch_response, kGzipMime1, &encoding_types);
  EXPECT_EQ(1U, encoding_types.size());
  EXPECT_EQ(Filter::FILTER_TYPE_SDCH, encoding_types.front());

  // Check to be sure that gzip can survive with other mime types.
  encoding_types.clear();
  encoding_types.push_back(Filter::FILTER_TYPE_GZIP);
  Filter::FixupEncodingTypes(is_sdch_response, "other/mime", &encoding_types);
  EXPECT_EQ(1U, encoding_types.size());
  EXPECT_EQ(Filter::FILTER_TYPE_GZIP, encoding_types.front());
}

TEST(FilterTest, SdchEncoding) {
  // Handle content encodings including SDCH.
  const bool is_sdch_response = true;
  const std::string kTextHtmlMime("text/html");
  std::vector<Filter::FilterType> encoding_types;

  // Check for most common encoding, and verify it survives unchanged.
  encoding_types.clear();
  encoding_types.push_back(Filter::FILTER_TYPE_SDCH);
  encoding_types.push_back(Filter::FILTER_TYPE_GZIP);
  Filter::FixupEncodingTypes(is_sdch_response, kTextHtmlMime, &encoding_types);
  EXPECT_EQ(2U, encoding_types.size());
  EXPECT_EQ(Filter::FILTER_TYPE_SDCH, encoding_types[0]);
  EXPECT_EQ(Filter::FILTER_TYPE_GZIP, encoding_types[1]);

  // Unchanged even with other mime types.
  encoding_types.clear();
  encoding_types.push_back(Filter::FILTER_TYPE_SDCH);
  encoding_types.push_back(Filter::FILTER_TYPE_GZIP);
  Filter::FixupEncodingTypes(is_sdch_response, "other/type", &encoding_types);
  EXPECT_EQ(2U, encoding_types.size());
  EXPECT_EQ(Filter::FILTER_TYPE_SDCH, encoding_types[0]);
  EXPECT_EQ(Filter::FILTER_TYPE_GZIP, encoding_types[1]);

  // Solo SDCH is extended to include optional gunzip.
  encoding_types.clear();
  encoding_types.push_back(Filter::FILTER_TYPE_SDCH);
  Filter::FixupEncodingTypes(is_sdch_response, "other/type", &encoding_types);
  EXPECT_EQ(2U, encoding_types.size());
  EXPECT_EQ(Filter::FILTER_TYPE_SDCH, encoding_types[0]);
  EXPECT_EQ(Filter::FILTER_TYPE_GZIP_HELPING_SDCH, encoding_types[1]);
}

TEST(FilterTest, MissingSdchEncoding) {
  // Handle interesting case where entire SDCH encoding assertion "got lost."
  const bool is_sdch_response = true;
  const std::string kTextHtmlMime("text/html");
  std::vector<Filter::FilterType> encoding_types;

  // Loss of encoding, but it was an SDCH response with html type.
  encoding_types.clear();
  Filter::FixupEncodingTypes(is_sdch_response, kTextHtmlMime, &encoding_types);
  EXPECT_EQ(2U, encoding_types.size());
  EXPECT_EQ(Filter::FILTER_TYPE_SDCH_POSSIBLE, encoding_types[0]);
  EXPECT_EQ(Filter::FILTER_TYPE_GZIP_HELPING_SDCH, encoding_types[1]);

  // No encoding, but it was an SDCH response with non-html type.
  encoding_types.clear();
  Filter::FixupEncodingTypes(is_sdch_response, "other/mime", &encoding_types);
  EXPECT_TRUE(encoding_types.empty());
}
