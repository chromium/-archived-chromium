// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/base_paths.h"
#include "base/file_util.h"
#include "media/base/yuv_scale.h"
#include "testing/gtest/include/gtest/gtest.h"

// Reference images were created with the following steps
// ffmpeg -vframes 25 -i bali.mov -vcodec rawvideo -pix_fmt yuv420p -an
//   bali.yv12.1280_720.yuv
// yuvhalf -yv12 -skip 24 bali.yv12.1280_720.yuv bali.yv12.640_360.yuv

// ffmpeg -vframes 25 -i bali.mov -vcodec rawvideo -pix_fmt yuv422p -an
//   bali.yv16.1280_720.yuv
// yuvhalf -yv16 -skip 24 bali.yv16.1280_720.yuv bali.yv16.640_360.yuv

// After running the scale functions in the unit test below, a hash
// is taken and compared to a reference value.
// If the image or code changes, the hash will require an update.
// A process outside the unittest is needed to confirm the new code
// and data are correctly working.  At this time, the author used
// the media_player with the same parameters and visually inspected the
// quality.  Once satisified that the quality is correct, the hash
// can be updated and used to ensure quality remains the same on
// all future builds and ports.

// Size of raw image.
static const int kWidth = 640;
static const int kHeight = 360;
static const int kScaledWidth = 1024;
static const int kScaledHeight = 768;
static const int kBpp = 4;

// DJB2 hash
unsigned int hash(unsigned char *s, size_t len) {
  unsigned int hash = 5381;
  while (len--)
    hash = hash * 33 + *s++;
  return hash;
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

  media::ScaleYV12ToRGB32(yuv_bytes,                             // Y plane
                          yuv_bytes + kWidth * kHeight,          // U plane
                          yuv_bytes + kWidth * kHeight * 5 / 4,  // V plane
                          rgb_scaled_bytes,                      // Rgb output
                          kWidth, kHeight,                       // Dimensions
                          kScaledWidth, kScaledHeight,           // Dimensions
                          kWidth,                                // YStride
                          kWidth / 2,                            // UvStride
                          kScaledWidth * kBpp,                   // RgbStride
                          media::ROTATE_0);

  unsigned int rgb_hash = hash(rgb_scaled_bytes, size_of_rgb_scaled);

  // To get this hash value, run once and examine the following EXPECT_EQ.
  // Then plug new hash value into EXPECT_EQ statements.

  EXPECT_EQ(rgb_hash, 1849654084u);
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

  media::ScaleYV16ToRGB32(yuv_bytes,                             // Y plane
                          yuv_bytes + kWidth * kHeight,          // U plane
                          yuv_bytes + kWidth * kHeight * 3 / 2,  // V plane
                          rgb_scaled_bytes,                      // Rgb output
                          kWidth, kHeight,                       // Dimensions
                          kScaledWidth, kScaledHeight,           // Dimensions
                          kWidth,                                // YStride
                          kWidth / 2,                            // UvStride
                          kScaledWidth * kBpp,                   // RgbStride
                          media::ROTATE_0);

  unsigned int rgb_hash = hash(rgb_scaled_bytes, size_of_rgb_scaled);

  // To get this hash value, run once and examine the following EXPECT_EQ.
  // Then plug new hash value into EXPECT_EQ statements.

  EXPECT_EQ(rgb_hash, 1297606858u);
  return;
}

