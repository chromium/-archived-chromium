// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/base_paths.h"
#include "base/file_util.h"
#include "media/base/yuv_convert.h"
#include "media/base/yuv_row.h"
#include "testing/gtest/include/gtest/gtest.h"

// Reference images were created with the following steps
// ffmpeg -vframes 25 -i bali.mov -vcodec rawvideo -pix_fmt yuv420p -an
//   bali.yv12.1280_720.yuv
// yuvhalf -yv12 -skip 24 bali.yv12.1280_720.yuv bali.yv12.640_360.yuv

// ffmpeg -vframes 25 -i bali.mov -vcodec rawvideo -pix_fmt yuv422p -an
//   bali.yv16.1280_720.yuv
// yuvhalf -yv16 -skip 24 bali.yv16.1280_720.yuv bali.yv16.640_360.yuv
// Size of raw image.

// Size of raw image.
static const int kWidth = 640;
static const int kHeight = 360;
static const int kScaledWidth = 1024;
static const int kScaledHeight = 768;
static const int kBpp = 4;

// Surface sizes.
static const size_t kYUV12Size = kWidth * kHeight * 12 / 8;
static const size_t kYUV16Size = kWidth * kHeight * 16 / 8;
static const size_t kRGBSize = kWidth * kHeight * kBpp;
static const size_t kRGBSizeConverted = kWidth * kHeight * kBpp;

namespace {
// DJB2 hash
unsigned int hash(unsigned char *s, size_t len, unsigned int hash = 5381) {
  while (len--)
    hash = hash * 33 + *s++;
  return hash;
}
}

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

  // Convert a frame of YUV to 32 bit ARGB.
  media::ConvertYUVToRGB32(yuv_bytes.get(),                             // Y
                           yuv_bytes.get() + kWidth * kHeight,          // U
                           yuv_bytes.get() + kWidth * kHeight * 5 / 4,  // V
                           rgb_converted_bytes.get(),             // RGB output
                           kWidth, kHeight,                       // Dimensions
                           kWidth,                                // YStride
                           kWidth / 2,                            // UVStride
                           kWidth * kBpp,                         // RGBStride
                           media::YV12);

  unsigned int rgb_hash = hash(rgb_converted_bytes.get(), kRGBSizeConverted);

  // To get this hash value, run once and examine the following EXPECT_EQ.
  // Then plug new hash value into EXPECT_EQ statements.

  // TODO(fbarchard): Make reference code mimic MMX exactly
#if USE_MMX
  EXPECT_EQ(2413171226u, rgb_hash);
#else
  EXPECT_EQ(2936300063u, rgb_hash);
#endif
  return;  // This is here to allow you to put a break point on this line
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

  // Convert a frame of YUV to 32 bit ARGB.
  media::ConvertYUVToRGB32(yuv_bytes.get(),                             // Y
                           yuv_bytes.get() + kWidth * kHeight,          // U
                           yuv_bytes.get() + kWidth * kHeight * 3 / 2,  // V
                           rgb_converted_bytes.get(),             // RGB output
                           kWidth, kHeight,                       // Dimensions
                           kWidth,                                // YStride
                           kWidth / 2,                            // UVStride
                           kWidth * kBpp,                         // RGBStride
                           media::YV16);

  unsigned int rgb_hash = hash(rgb_converted_bytes.get(), kRGBSizeConverted);

  // To get this hash value, run once and examine the following EXPECT_EQ.
  // Then plug new hash value into EXPECT_EQ statements.

  // TODO(fbarchard): Make reference code mimic MMX exactly
#if USE_MMX
  EXPECT_EQ(4222342047u, rgb_hash);
#else
  EXPECT_EQ(106869773u, rgb_hash);
#endif
  return;  // This is here to allow you to put a break point on this line
}

TEST(YuvScaleTest, Basic) {
  // Read YUV reference data from file.
  FilePath yuv_url;
  EXPECT_TRUE(PathService::Get(base::DIR_SOURCE_ROOT, &yuv_url));
  yuv_url = yuv_url.Append(FILE_PATH_LITERAL("media"))
                   .Append(FILE_PATH_LITERAL("test"))
                   .Append(FILE_PATH_LITERAL("data"))
                   .Append(FILE_PATH_LITERAL("bali.yv12.640_360.yuv"));
  const size_t size_of_yuv = kWidth * kHeight * 12 / 8;  // 12 bpp.
  uint8* yuv_bytes = new uint8[size_of_yuv];
  EXPECT_EQ(static_cast<int>(size_of_yuv),
            file_util::ReadFile(yuv_url,
                                reinterpret_cast<char*>(yuv_bytes),
                                static_cast<int>(size_of_yuv)));

  // Scale a frame of YUV to 32 bit ARGB.
  const size_t size_of_rgb_scaled = kScaledWidth * kScaledHeight * kBpp;
  uint8* rgb_scaled_bytes = new uint8[size_of_rgb_scaled];

  media::ScaleYUVToRGB32(yuv_bytes,                             // Y plane
                         yuv_bytes + kWidth * kHeight,          // U plane
                         yuv_bytes + kWidth * kHeight * 5 / 4,  // V plane
                         rgb_scaled_bytes,                      // Rgb output
                         kWidth, kHeight,                       // Dimensions
                         kScaledWidth, kScaledHeight,           // Dimensions
                         kWidth,                                // YStride
                         kWidth / 2,                            // UvStride
                         kScaledWidth * kBpp,                   // RgbStride
                         media::YV12,
                         media::ROTATE_0);

  unsigned int rgb_hash = hash(rgb_scaled_bytes, size_of_rgb_scaled);

  // To get this hash value, run once and examine the following EXPECT_EQ.
  // Then plug new hash value into EXPECT_EQ statements.

  // TODO(fbarchard): Make reference code mimic MMX exactly
#if USE_MMX
  EXPECT_EQ(4259656254u, rgb_hash);
#else
  EXPECT_EQ(197274901u, rgb_hash);
#endif
  return;  // This is here to allow you to put a break point on this line
}

TEST(YV16ScaleTest, Basic) {
  // Read YV16 reference data from file.
  FilePath yuv_url;
  EXPECT_TRUE(PathService::Get(base::DIR_SOURCE_ROOT, &yuv_url));
  yuv_url = yuv_url.Append(FILE_PATH_LITERAL("media"))
                   .Append(FILE_PATH_LITERAL("test"))
                   .Append(FILE_PATH_LITERAL("data"))
                   .Append(FILE_PATH_LITERAL("bali.yv16.640_360.yuv"));
  const size_t size_of_yuv = kWidth * kHeight * 16 / 8;  // 16 bpp.
  uint8* yuv_bytes = new uint8[size_of_yuv];
  EXPECT_EQ(static_cast<int>(size_of_yuv),
            file_util::ReadFile(yuv_url,
                                reinterpret_cast<char*>(yuv_bytes),
                                static_cast<int>(size_of_yuv)));

  // Scale a frame of YUV to 32 bit ARGB.
  const size_t size_of_rgb_scaled = kScaledWidth * kScaledHeight * kBpp;
  uint8* rgb_scaled_bytes = new uint8[size_of_rgb_scaled];

  media::ScaleYUVToRGB32(yuv_bytes,                             // Y plane
                         yuv_bytes + kWidth * kHeight,          // U plane
                         yuv_bytes + kWidth * kHeight * 3 / 2,  // V plane
                         rgb_scaled_bytes,                      // Rgb output
                         kWidth, kHeight,                       // Dimensions
                         kScaledWidth, kScaledHeight,           // Dimensions
                         kWidth,                                // YStride
                         kWidth / 2,                            // UvStride
                         kScaledWidth * kBpp,                   // RgbStride
                         media::YV16,
                         media::ROTATE_0);

  unsigned int rgb_hash = hash(rgb_scaled_bytes, size_of_rgb_scaled);

  // To get this hash value, run once and examine the following EXPECT_EQ.
  // Then plug new hash value into EXPECT_EQ statements.

  // TODO(fbarchard): Make reference code mimic MMX exactly
#if USE_MMX
  EXPECT_EQ(974965419u, rgb_hash);
#else
  EXPECT_EQ(2946450771u, rgb_hash);
#endif
  return;
}

