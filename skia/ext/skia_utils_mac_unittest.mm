// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "skia/ext/skia_utils_mac.mm"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class SkiaUtilsMacTest : public testing::Test {
 public:
  // If not red, is blue.
  // If not tfbit (twenty-four-bit), is 444.
  void ShapeHelper(int width, int height, bool isred, bool tfbit);
};

void SkiaUtilsMacTest::ShapeHelper(int width, int height,
                                   bool isred, bool tfbit) {
  SkBitmap thing;

  if (tfbit)
    thing.setConfig(SkBitmap::kARGB_8888_Config, width, height);
  else
    thing.setConfig(SkBitmap::kARGB_4444_Config, width, height);
  thing.allocPixels();

  if (isred)
    thing.eraseRGB(0xff, 0, 0);
  else
    thing.eraseRGB(0, 0, 0xff);

  // Confirm size
  NSImage* image = gfx::SkBitmapToNSImage(thing);
  EXPECT_DOUBLE_EQ([image size].width, (double)width);
  EXPECT_DOUBLE_EQ([image size].height, (double)height);

  // Get the color of a pixel and make sure it looks fine
  [image lockFocus];

  int x = width > 17 ? 17 : 0;
  int y = height > 17 ? 17 : 0;
  NSColor* color = NSReadPixel(NSMakePoint(x, y));
  CGFloat red = 0, green = 0, blue = 0, alpha = 0;
  [image unlockFocus];
  [color getRed:&red green:&green blue:&blue alpha:&alpha];

  // Be tolerant of floating point rounding, gamma, etc.
  if (isred) {
    EXPECT_GT(red, 0.8);
    EXPECT_LT(blue, 0.2);
  } else {
    EXPECT_LT(red, 0.2);
    EXPECT_GT(blue, 0.8);
  }
  EXPECT_LT(green, 0.2);
  EXPECT_GT(alpha, 0.9);
}

TEST_F(SkiaUtilsMacTest, BitmapToNSImage_RedSquare64x64) {
  ShapeHelper(64, 64, true, true);
}

TEST_F(SkiaUtilsMacTest, BitmapToNSImage_BlueRectangle199x19) {
  ShapeHelper(199, 19, false, true);
}

TEST_F(SkiaUtilsMacTest, BitmapToNSImage_BlueRectangle444) {
  ShapeHelper(200, 200, false, false);
}

}  // namespace
