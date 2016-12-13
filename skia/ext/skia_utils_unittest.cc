// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdlib.h>

#include "skia/ext/skia_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColorPriv.h"

TEST(SkiaUtils, SkColorToHSLRed) {
  SkColor red = SkColorSetARGB(255, 255, 0, 0);
  skia::HSL hsl = { 0, 0, 0 };
  skia::SkColorToHSL(red, hsl);
  EXPECT_EQ(hsl.h, 0);
  EXPECT_EQ(hsl.s, 1);
  EXPECT_EQ(hsl.l, 0.5);
}

TEST(SkiaUtils, SkColorToHSLGrey) {
  SkColor red = SkColorSetARGB(255, 128, 128, 128);
  skia::HSL hsl = { 0, 0, 0 };
  skia::SkColorToHSL(red, hsl);
  EXPECT_EQ(hsl.h, 0);
  EXPECT_EQ(hsl.s, 0);
  EXPECT_EQ(static_cast<int>(hsl.l * 100),
            static_cast<int>(0.5 * 100));  // Accurate to two decimal places.
}

TEST(SkiaUtils, HSLToSkColorWithAlpha) {
  // Premultiplied alpha - this is full red.
  SkColor red = SkColorSetARGB(128, 128, 0, 0);

  skia::HSL hsl = { 0, 1, 0.5 };

  SkColor result = skia::HSLToSKColor(128, hsl);
  EXPECT_EQ(SkColorGetA(red), SkColorGetA(result));
  EXPECT_EQ(SkColorGetR(red), SkColorGetR(result));
  EXPECT_EQ(SkColorGetG(red), SkColorGetG(result));
  EXPECT_EQ(SkColorGetB(red), SkColorGetB(result));
}

