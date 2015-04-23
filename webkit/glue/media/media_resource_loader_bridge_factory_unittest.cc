// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_util.h"
#include "webkit/glue/media/media_resource_loader_bridge_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace webkit_glue {

TEST(MediaResourceLoaderBridgeFactoryTest, GenerateHeaders) {
  static const struct {
    const bool success;
    const int64 first_byte_position;
    const int64 last_byte_position;
  } data[] = {
    { false, -1, -1 },
    { false, -5, 0 },
    { false, 100, 0 },
    { true, 0, -1 },
    { true, 0, 0 },
    { true, 100, 100 },
    { true, 50, -1 },
    { true, 10000, -1 },
    { true, 50, 100 },
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(data); ++i) {
    std::string headers = MediaResourceLoaderBridgeFactory::GenerateHeaders(
        data[i].first_byte_position, data[i].last_byte_position);
    std::vector<net::HttpByteRange> ranges;
    bool ret = net::HttpUtil::ParseRanges(headers, &ranges);
    EXPECT_EQ(data[i].success, ret);
    if (ret) {
      EXPECT_EQ(1u, ranges.size());
      EXPECT_EQ(data[i].first_byte_position,
                ranges[0].first_byte_position());
      EXPECT_EQ(data[i].last_byte_position,
                ranges[0].last_byte_position());
    }
  }
}

}  // namespace webkit_glue
