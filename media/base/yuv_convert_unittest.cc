// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/base_paths.h"
#include "base/file_util.h"
#include "media/base/yuv_convert.h"
#include "testing/gtest/include/gtest/gtest.h"

// Reference images were created with the following steps
// ffmpeg -vframes 25 -i bali.mov -vcodec rawvideo -pix_fmt yuv420p -an
//   bali.yv12.1280_720.yuv
// yuvhalf -yv12 -skip 24 bali.yv12.1280_720.yuv bali.yv12.640_360.yuv
// yuvtool -yv12 bali.yv12.640_360.yuv bali.yv12.640_360.rgb

// ffmpeg -vframes 25 -i bali.mov -vcodec rawvideo -pix_fmt yuv422p -an
//   bali.yv16.1280_720.yuv
// yuvhalf -yv16 -skip 24 bali.yv16.1280_720.yuv bali.yv16.640_360.yuv
// yuvtool -yv16 bali.yv16.640_360.yuv bali.yv16.640_360.rgb

// Size of raw image.
static const int kWidth = 640;
static const int kHeight = 360;
static const int kBpp = 4;

// Surface sizes.
static const size_t kYUV12Size = kWidth * kHeight * 12 / 8;
static const size_t kYUV16Size = kWidth * kHeight * 16 / 8;
static const size_t kRGBSize = kWidth * kHeight * kBpp;
static const size_t kRGBSizeConverted = kWidth * kHeight * kBpp;

TEST(YUVConvertTest, YV12) {
  // Allocate all surfaces.
  scoped_array<uint8> yuv_bytes(new uint8[kYUV12Size]);
  scoped_array<uint8> rgb_bytes(new uint8[kRGBSize]);
  scoped_array<uint8> rgb_converted_bytes(new uint8[kRGBSizeConverted]);

  // Read YUV reference data from file.
  FilePath yuv_url;
  EXPECT_TRUE(PathService::Get(base::DIR_SOURCE_ROOT, &yuv_url));
  yuv_url = yuv_url.Append(FILE_PATH_LITERAL("media"))
                   .Append(FILE_PATH_LITERAL("test"))
                   .Append(FILE_PATH_LITERAL("data"))
                   .Append(FILE_PATH_LITERAL("bali.yv12.640_360.yuv"));
  EXPECT_EQ(static_cast<int>(kYUV12Size),
            file_util::ReadFile(yuv_url,
                                reinterpret_cast<char*>(yuv_bytes.get()),
                                static_cast<int>(kYUV12Size)));

  // Read RGB reference data from file.
  FilePath rgb_url;
  EXPECT_TRUE(PathService::Get(base::DIR_SOURCE_ROOT, &rgb_url));
  rgb_url = rgb_url.Append(FILE_PATH_LITERAL("media"))
                   .Append(FILE_PATH_LITERAL("test"))
                   .Append(FILE_PATH_LITERAL("data"))
                   .Append(FILE_PATH_LITERAL("bali.yv12.640_360.rgb"));
  EXPECT_EQ(static_cast<int>(kRGBSize),
            file_util::ReadFile(rgb_url,
                                reinterpret_cast<char*>(rgb_bytes.get()),
                                static_cast<int>(kRGBSize)));

  // Convert a frame of YUV to 32 bit ARGB.
  media::ConvertYV12ToRGB32(yuv_bytes.get(),                             // Y
                            yuv_bytes.get() + kWidth * kHeight,          // U
                            yuv_bytes.get() + kWidth * kHeight * 5 / 4,  // V
                            rgb_converted_bytes.get(),             // RGB output
                            kWidth, kHeight,                       // Dimensions
                            kWidth,                                // YStride
                            kWidth / 2,                            // UVStride
                            kWidth * kBpp);                        // RGBStride

  // Compare converted YUV to reference conversion file.
  int rgb_diff = memcmp(rgb_converted_bytes.get(), rgb_bytes.get(), kRGBSize);

  EXPECT_EQ(rgb_diff, 0);
}

TEST(YUVConvertTest, YV16) {
  // Allocate all surfaces.
  scoped_array<uint8> yuv_bytes(new uint8[kYUV16Size]);
  scoped_array<uint8> rgb_bytes(new uint8[kRGBSize]);
  scoped_array<uint8> rgb_converted_bytes(new uint8[kRGBSizeConverted]);

  // Read YV16 reference data from file.
  FilePath yuv_url;
  EXPECT_TRUE(PathService::Get(base::DIR_SOURCE_ROOT, &yuv_url));
  yuv_url = yuv_url.Append(FILE_PATH_LITERAL("media"))
                   .Append(FILE_PATH_LITERAL("test"))
                   .Append(FILE_PATH_LITERAL("data"))
                   .Append(FILE_PATH_LITERAL("bali.yv16.640_360.yuv"));
  EXPECT_EQ(static_cast<int>(kYUV16Size),
            file_util::ReadFile(yuv_url,
                                reinterpret_cast<char*>(yuv_bytes.get()),
                                static_cast<int>(kYUV16Size)));

  // Read RGB reference data from file.
  FilePath rgb_url;
  EXPECT_TRUE(PathService::Get(base::DIR_SOURCE_ROOT, &rgb_url));
  rgb_url = rgb_url.Append(FILE_PATH_LITERAL("media"))
                   .Append(FILE_PATH_LITERAL("test"))
                   .Append(FILE_PATH_LITERAL("data"))
                   .Append(FILE_PATH_LITERAL("bali.yv16.640_360.rgb"));
  EXPECT_EQ(static_cast<int>(kRGBSize),
            file_util::ReadFile(rgb_url,
                                reinterpret_cast<char*>(rgb_bytes.get()),
                                static_cast<int>(kRGBSize)));

  // Convert a frame of YUV to 32 bit ARGB.
  media::ConvertYV16ToRGB32(yuv_bytes.get(),                             // Y
                            yuv_bytes.get() + kWidth * kHeight,          // U
                            yuv_bytes.get() + kWidth * kHeight * 3 / 2,  // V
                            rgb_converted_bytes.get(),             // RGB output
                            kWidth, kHeight,                       // Dimensions
                            kWidth,                                // YStride
                            kWidth / 2,                            // UVStride
                            kWidth * kBpp);                        // RGBStride

  // Compare converted YUV to reference conversion file.
  int rgb_diff = memcmp(rgb_converted_bytes.get(), rgb_bytes.get(), kRGBSize);

  EXPECT_EQ(rgb_diff, 0);
}
