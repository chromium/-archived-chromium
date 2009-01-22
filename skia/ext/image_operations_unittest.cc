// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdlib.h>

#include "skia/ext/image_operations.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "SkBitmap.h"

namespace {

// Computes the average pixel value for the given range, inclusive.
uint32_t AveragePixel(const SkBitmap& bmp,
                      int x_min, int x_max,
                      int y_min, int y_max) {
  float accum[4] = {0, 0, 0, 0};
  int count = 0;
  for (int y = y_min; y <= y_max; y++) {
    for (int x = x_min; x <= x_max; x++) {
      uint32_t cur = *bmp.getAddr32(x, y);
      accum[0] += SkColorGetB(cur);
      accum[1] += SkColorGetG(cur);
      accum[2] += SkColorGetR(cur);
      accum[3] += SkColorGetA(cur);
      count++;
    }
  }

  return SkColorSetARGB(static_cast<unsigned char>(accum[3] / count),
                        static_cast<unsigned char>(accum[2] / count),
                        static_cast<unsigned char>(accum[1] / count),
                        static_cast<unsigned char>(accum[0] / count));
}

// Returns true if each channel of the given two colors are "close." This is
// used for comparing colors where rounding errors may cause off-by-one.
bool ColorsClose(uint32_t a, uint32_t b) {
  return abs(static_cast<int>(SkColorGetB(a) - SkColorGetB(b))) < 2 &&
         abs(static_cast<int>(SkColorGetG(a) - SkColorGetG(b))) < 2 &&
         abs(static_cast<int>(SkColorGetR(a) - SkColorGetR(b))) < 2 &&
         abs(static_cast<int>(SkColorGetA(a) - SkColorGetA(b))) < 2;
}

void FillDataToBitmap(int w, int h, SkBitmap* bmp) {
  bmp->setConfig(SkBitmap::kARGB_8888_Config, w, h);
  bmp->allocPixels();

  unsigned char* src_data =
      reinterpret_cast<unsigned char*>(bmp->getAddr32(0, 0));
  for (int i = 0; i < w * h; i++) {
    src_data[i * 4 + 0] = static_cast<unsigned char>(i % 255);
    src_data[i * 4 + 1] = static_cast<unsigned char>(i % 255);
    src_data[i * 4 + 2] = static_cast<unsigned char>(i % 255);
    src_data[i * 4 + 3] = static_cast<unsigned char>(i % 255);
  }
}

}  // namespace

// Makes the bitmap 50% the size as the original using a box filter. This is
// an easy operation that we can check the results for manually.
TEST(ImageOperations, Halve) {
  // Make our source bitmap.
  int src_w = 30, src_h = 38;
  SkBitmap src;
  FillDataToBitmap(src_w, src_h, &src);

  // Do a halving of the full bitmap.
  SkBitmap actual_results = skia::ImageOperations::Resize(
      src, skia::ImageOperations::RESIZE_BOX, src_w / 2, src_h / 2);
  ASSERT_EQ(src_w / 2, actual_results.width());
  ASSERT_EQ(src_h / 2, actual_results.height());

  // Compute the expected values & compare.
  SkAutoLockPixels lock(actual_results);
  for (int y = 0; y < actual_results.height(); y++) {
    for (int x = 0; x < actual_results.width(); x++) {
      int first_x = std::max(0, x * 2 - 1);
      int last_x = std::min(src_w - 1, x * 2);

      int first_y = std::max(0, y * 2 - 1);
      int last_y = std::min(src_h - 1, y * 2);

      uint32_t expected_color = AveragePixel(src,
                                             first_x, last_x, first_y, last_y);
      EXPECT_TRUE(ColorsClose(expected_color, *actual_results.getAddr32(x, y)));
    }
  }
}

TEST(ImageOperations, HalveSubset) {
  // Make our source bitmap.
  int src_w = 16, src_h = 34;
  SkBitmap src;
  FillDataToBitmap(src_w, src_h, &src);

  // Do a halving of the full bitmap.
  SkBitmap full_results = skia::ImageOperations::Resize(
      src, skia::ImageOperations::RESIZE_BOX, src_w / 2, src_h / 2);
  ASSERT_EQ(src_w / 2, full_results.width());
  ASSERT_EQ(src_h / 2, full_results.height());

  // Now do a halving of a a subset, recall the destination subset is in the
  // destination coordinate system (max = half of the original image size).
  gfx::Rect subset_rect(2, 3, 3, 6);
  SkBitmap subset_results = skia::ImageOperations::Resize(
      src, skia::ImageOperations::RESIZE_BOX,
      src_w / 2, src_h / 2, subset_rect);
  ASSERT_EQ(subset_rect.width(), subset_results.width());
  ASSERT_EQ(subset_rect.height(), subset_results.height());

  // The computed subset and the corresponding subset of the original image
  // should be the same.
  SkAutoLockPixels full_lock(full_results);
  SkAutoLockPixels subset_lock(subset_results);
  for (int y = 0; y < subset_rect.height(); y++) {
    for (int x = 0; x < subset_rect.width(); x++) {
      ASSERT_EQ(
          *full_results.getAddr32(x + subset_rect.x(), y + subset_rect.y()),
          *subset_results.getAddr32(x, y));
    }
  }
}

// Resamples an iamge to the same image, it should give almost the same result.
TEST(ImageOperations, ResampleToSame) {
  // Make our source bitmap.
  int src_w = 16, src_h = 34;
  SkBitmap src;
  FillDataToBitmap(src_w, src_h, &src);

  // Do a resize of the full bitmap to the same size. The lanczos filter is good
  // enough that we should get exactly the same image for output.
  SkBitmap results = skia::ImageOperations::Resize(
      src, skia::ImageOperations::RESIZE_LANCZOS3, src_w, src_h);
  ASSERT_EQ(src_w, results.width());
  ASSERT_EQ(src_h, results.height());

  SkAutoLockPixels src_lock(src);
  SkAutoLockPixels results_lock(results);
  for (int y = 0; y < src_h; y++) {
    for (int x = 0; x < src_w; x++) {
      EXPECT_EQ(*src.getAddr32(x, y), *results.getAddr32(x, y));
    }
  }
}
