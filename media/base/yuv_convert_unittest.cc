// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/base_paths.h"
#include "base/file_util.h"
#include "media/base/yuv_convert.h"
#include "testing/gtest/include/gtest/gtest.h"

// Size of raw image.
static const int kWidth = 1280;
static const int kHeight = 720;
static const int kBpp = 4;

TEST(YuvConvertTest, Basic) {
  // Read YUV reference data from file.
  FilePath yuv_url;
  EXPECT_TRUE(PathService::Get(base::DIR_SOURCE_ROOT, &yuv_url));
  yuv_url = yuv_url.Append(FILE_PATH_LITERAL("media"))
                   .Append(FILE_PATH_LITERAL("test"))
                   .Append(FILE_PATH_LITERAL("data"))
                   .Append(FILE_PATH_LITERAL("yuv_file"));
  const size_t size_of_yuv = kWidth * kHeight * 12 / 8;
  uint8* yuv_bytes = new uint8[size_of_yuv];
  EXPECT_EQ(static_cast<int>(size_of_yuv),
            file_util::ReadFile(yuv_url,
                                reinterpret_cast<char*>(yuv_bytes),
                                static_cast<int>(size_of_yuv)));

  // Read RGB reference data from file.
  // To keep the file smaller, only the first quarter of the file
  // is checked into SVN.
  FilePath rgb_url;
  EXPECT_TRUE(PathService::Get(base::DIR_SOURCE_ROOT, &rgb_url));
  rgb_url = rgb_url.Append(FILE_PATH_LITERAL("media"))
                   .Append(FILE_PATH_LITERAL("test"))
                   .Append(FILE_PATH_LITERAL("data"))
                   .Append(FILE_PATH_LITERAL("rgb_file"));
  const size_t size_of_rgb = kWidth * kHeight * kBpp / 4;
  uint8* rgb_bytes = new uint8[size_of_rgb];
  EXPECT_EQ(static_cast<int>(size_of_rgb),
            file_util::ReadFile(rgb_url,
                                reinterpret_cast<char*>(rgb_bytes),
                                static_cast<int>(size_of_rgb)));

  // Convert a frame of YUV to 32 bit ARGB.
  const size_t size_of_rgb_converted = kWidth * kHeight * kBpp;
  uint8* rgb_converted_bytes = new uint8[size_of_rgb_converted];

  media::ConvertYV12ToRGB32(yuv_bytes,                             // Y plane
                            yuv_bytes + kWidth * kHeight,          // U plane
                            yuv_bytes + kWidth * kHeight * 5 / 4,  // V plane
                            rgb_converted_bytes,                   // Rgb output
                            kWidth, kHeight,                       // Dimensions
                            kWidth,                                // YStride
                            kWidth / 2,                            // UvStride
                            kWidth * kBpp);                        // RgbStride

  // Compare converted YUV to reference conversion file.
  int rgb_diff = memcmp(rgb_converted_bytes, rgb_bytes, size_of_rgb);

  EXPECT_EQ(rgb_diff, 0);
}

