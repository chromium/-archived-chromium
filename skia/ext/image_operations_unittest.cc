// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdlib.h>

#include "skia/ext/image_operations.h"
#include "skia/include/SkColorPriv.h"
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

// Blend two bitmaps together at 50% alpha and verify that the result
// is the middle-blend of the two.
TEST(ImageOperations, CreateBlendedBitmap) {
  int src_w = 16, src_h = 16;
  SkBitmap src_a;
  src_a.setConfig(SkBitmap::kARGB_8888_Config, src_w, src_h);
  src_a.allocPixels();

  SkBitmap src_b;
  src_b.setConfig(SkBitmap::kARGB_8888_Config, src_w, src_h);
  src_b.allocPixels();

  for (int y = 0, i = 0; y < src_h; y++) {
    for (int x = 0; x < src_w; x++) {
      *src_a.getAddr32(x, y) = SkColorSetARGB(255, 0, i * 2 % 255, i % 255);
      *src_b.getAddr32(x, y) =
          SkColorSetARGB((255 - i) % 255, i % 255, i * 4 % 255, 0);
      i++;
    }
  }

  // Shift to red.
  SkBitmap blended = skia::ImageOperations::CreateBlendedBitmap(
    src_a, src_b, 0.5);
  SkAutoLockPixels srca_lock(src_a);
  SkAutoLockPixels srcb_lock(src_b);
  SkAutoLockPixels blended_lock(blended);

  for (int y = 0; y < src_h; y++) {
    for (int x = 0; x < src_w; x++) {
      int i = y * src_w + x;
      EXPECT_EQ((255 + ((255 - i) % 255)) / 2,
                SkColorGetA(*blended.getAddr32(x, y)));
      EXPECT_EQ(i % 255 / 2,
                SkColorGetR(*blended.getAddr32(x, y)));
      EXPECT_EQ(((i * 2) % 255 + (i * 4) % 255) / 2,
                SkColorGetG(*blended.getAddr32(x, y)));
      EXPECT_EQ(i % 255 / 2,
                SkColorGetB(*blended.getAddr32(x, y)));
    }
  }
}

// Test our masking functions.
TEST(ImageOperations, CreateMaskedBitmap) {
  int src_w = 16, src_h = 16;

  SkBitmap src;
  FillDataToBitmap(src_w, src_h, &src);

  // Generate alpha mask
  SkBitmap alpha;
  alpha.setConfig(SkBitmap::kARGB_8888_Config, src_w, src_h);
  alpha.allocPixels();

  unsigned char* src_data =
      reinterpret_cast<unsigned char*>(alpha.getAddr32(0, 0));
  for (int i = 0; i < src_w * src_h; i++) {
    src_data[i * 4] = SkColorSetARGB(i + 128 % 255,
                                     i + 128 % 255,
                                     i + 64 % 255,
                                     i + 0 % 255);
  }

  SkBitmap masked = skia::ImageOperations::CreateMaskedBitmap(src, alpha);

  SkAutoLockPixels src_lock(src);
  SkAutoLockPixels masked_lock(masked);
  for (int y = 0; y < src_h; y++) {
    for (int x = 0; x < src_w; x++) {
      // Test that the alpha is equal.
      SkColor src_pixel = *src.getAddr32(x, y);
      SkColor alpha_pixel = *alpha.getAddr32(x, y);
      SkColor masked_pixel = *masked.getAddr32(x, y);

      // Test that the alpha is equal.
      int alpha = (alpha_pixel & 0xff000000) >> SK_A32_SHIFT;
      EXPECT_EQ(alpha, (masked_pixel & 0xff000000) >> SK_A32_SHIFT);

      // Test that the colors are right - SkBitmaps have premultiplied alpha,
      // so we can't just do a direct comparison.
      EXPECT_EQ(SkColorGetR(masked_pixel),
                SkAlphaMul(SkColorGetR(src_pixel), alpha));
    }
  }
}

// Testing blur without reimplementing the blur algorithm here is tough,
// so we just check to see if the pixels have moved in the direction we
// think they should move in (and also checking the wrapping behavior).
// This will allow us to tweak the blur algorithm to suit speed/visual
// needs without breaking the fundamentals.
TEST(ImageOperations, CreateBlurredBitmap) {
  int src_w = 4, src_h = 4;
  SkBitmap src;
  src.setConfig(SkBitmap::kARGB_8888_Config, src_w, src_h);
  src.allocPixels();

  for (int y = 0, i = 0; y < src_h; y++) {
    for (int x = 0; x < src_w; x++) {
      int r = (y == 0) ? 255 : 0;  // Make the top row red.
      int g = (i % 2 == 0) ? 255 : 0;  // Make green alternate in each pixel.
      int b = (y == src_h - 1) ? 255 : 0;  // Make the bottom row blue.

      *src.getAddr32(x, y) = SkColorSetARGB(255, r, g, b);
      i++;
    }
  }

  // Perform a small blur (enough to shove the values in the direction we
  // need - more would just be an unneccessary unit test slowdown).
  SkBitmap blurred = skia::ImageOperations::CreateBlurredBitmap(src, 2);

  SkAutoLockPixels src_lock(src);
  SkAutoLockPixels blurred_lock(blurred);
  for (int y = 0, i = 0; y < src_w; y++) {
    for (int x = 0; x < src_h; x++) {
      SkColor src_pixel = *src.getAddr32(x, y);
      SkColor blurred_pixel = *blurred.getAddr32(x, y);
      if (y == 0) {
        // We expect our red to have decreased, but our blue to have
        // increased (from the wrapping from the bottom line).
        EXPECT_TRUE(SkColorGetR(blurred_pixel) < SkColorGetR(src_pixel));
        EXPECT_TRUE(SkColorGetB(blurred_pixel) > SkColorGetB(src_pixel));
      } else if (y == src_h - 1) {
        // Now for the opposite.
        EXPECT_TRUE(SkColorGetB(blurred_pixel) < SkColorGetB(src_pixel));
        EXPECT_TRUE(SkColorGetR(blurred_pixel) > SkColorGetR(src_pixel));
      }

      // Expect the green channel to have moved towards the center (but
      // not past it).
      if (i % 2 == 0) {
        EXPECT_LT(SkColorGetG(blurred_pixel), SkColorGetG(src_pixel));
        EXPECT_GE(SkColorGetG(blurred_pixel), static_cast<uint32>(128));
      } else {
        EXPECT_GT(SkColorGetG(blurred_pixel), SkColorGetG(src_pixel));
        EXPECT_LE(SkColorGetG(blurred_pixel), static_cast<uint32>(128));
      }

      i++;
    }
  }
}

// Make sure that when shifting a bitmap without any shift parameters,
// the end result is close enough to the original (rounding errors
// notwithstanding).
TEST(ImageOperations, CreateHSLShiftedBitmapToSame) {
  int src_w = 4, src_h = 4;
  SkBitmap src;
  src.setConfig(SkBitmap::kARGB_8888_Config, src_w, src_h);
  src.allocPixels();

  for (int y = 0, i = 0; y < src_h; y++) {
    for (int x = 0; x < src_w; x++) {
      *src.getAddr32(x, y) = SkColorSetARGB(i + 128 % 255,
          i + 128 % 255, i + 64 % 255, i + 0 % 255);
      i++;
    }
  }

  float hsl[3] = { -1, -1, -1 };

  SkBitmap shifted = skia::ImageOperations::CreateHSLShiftedBitmap(src, hsl);

  SkAutoLockPixels src_lock(src);
  SkAutoLockPixels shifted_lock(shifted);

  for (int y = 0; y < src_w; y++) {
    for (int x = 0; x < src_h; x++) {
      SkColor src_pixel = *src.getAddr32(x, y);
      SkColor shifted_pixel = *shifted.getAddr32(x, y);
      EXPECT_TRUE(ColorsClose(src_pixel, shifted_pixel));
    }
  }
}

// Shift a blue bitmap to red.
TEST(ImageOperations, CreateHSLShiftedBitmapHueOnly) {
  int src_w = 16, src_h = 16;
  SkBitmap src;
  src.setConfig(SkBitmap::kARGB_8888_Config, src_w, src_h);
  src.allocPixels();

  for (int y = 0, i = 0; y < src_h; y++) {
    for (int x = 0; x < src_w; x++) {
      *src.getAddr32(x, y) = SkColorSetARGB(255, 0, 0, i % 255);
      i++;
    }
  }

  // Shift to red.
  float hsl[3] = { 0, -1, -1 };

  SkBitmap shifted = skia::ImageOperations::CreateHSLShiftedBitmap(src, hsl);

  SkAutoLockPixels src_lock(src);
  SkAutoLockPixels shifted_lock(shifted);

  for (int y = 0, i = 0; y < src_h; y++) {
    for (int x = 0; x < src_w; x++) {
      EXPECT_TRUE(ColorsClose(*shifted.getAddr32(x, y),
                              SkColorSetARGB(255, i % 255, 0, 0)));
      i++;
    }
  }
}

// Test our cropping.
TEST(ImageOperations, CreateCroppedBitmap) {
  int src_w = 16, src_h = 16;
  SkBitmap src;
  FillDataToBitmap(src_w, src_h, &src);

  SkBitmap cropped = skia::ImageOperations::CreateTiledBitmap(src, 4, 4,
                                                              8, 8);
  ASSERT_EQ(8, cropped.width());
  ASSERT_EQ(8, cropped.height());

  SkAutoLockPixels src_lock(src);
  SkAutoLockPixels cropped_lock(cropped);
  for (int y = 4; y < 12; y++) {
    for (int x = 4; x < 12; x++) {
      EXPECT_EQ(*src.getAddr32(x, y),
                *cropped.getAddr32(x - 4, y - 4));
    }
  }
}

// Test whether our cropping correctly wraps across image boundaries.
TEST(ImageOperations, CreateCroppedBitmapWrapping) {
  int src_w = 16, src_h = 16;
  SkBitmap src;
  FillDataToBitmap(src_w, src_h, &src);

  SkBitmap cropped = skia::ImageOperations::CreateTiledBitmap(
      src, src_w / 2, src_h / 2, src_w, src_h);
  ASSERT_EQ(src_w, cropped.width());
  ASSERT_EQ(src_h, cropped.height());

  SkAutoLockPixels src_lock(src);
  SkAutoLockPixels cropped_lock(cropped);
  for (int y = 0; y < src_h; y++) {
    for (int x = 0; x < src_w; x++) {
      EXPECT_EQ(*src.getAddr32(x, y),
                *cropped.getAddr32((x + src_w / 2) % src_w,
                                   (y + src_h / 2) % src_h));
    }
  }
}

