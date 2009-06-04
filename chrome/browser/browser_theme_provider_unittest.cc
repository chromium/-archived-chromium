// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browser_theme_provider.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(BrowserThemeProviderTest, AlignmentConversion) {
  // Verify that we get out what we put in.
  std::string top_left = "top left";
  int alignment = BrowserThemeProvider::StringToAlignment(top_left);
  EXPECT_EQ(BrowserThemeProvider::ALIGN_TOP | BrowserThemeProvider::ALIGN_LEFT,
            alignment);
  EXPECT_EQ(top_left, BrowserThemeProvider::AlignmentToString(alignment));

  alignment = BrowserThemeProvider::StringToAlignment("top");
  EXPECT_EQ(BrowserThemeProvider::ALIGN_TOP, alignment);
  EXPECT_EQ("top", BrowserThemeProvider::AlignmentToString(alignment));

  alignment = BrowserThemeProvider::StringToAlignment("left");
  EXPECT_EQ(BrowserThemeProvider::ALIGN_LEFT, alignment);
  EXPECT_EQ("left", BrowserThemeProvider::AlignmentToString(alignment));

  alignment = BrowserThemeProvider::StringToAlignment("right");
  EXPECT_EQ(BrowserThemeProvider::ALIGN_RIGHT, alignment);
  EXPECT_EQ("right", BrowserThemeProvider::AlignmentToString(alignment));

  alignment = BrowserThemeProvider::StringToAlignment("righttopbottom");
  EXPECT_EQ(BrowserThemeProvider::ALIGN_CENTER, alignment);
  EXPECT_EQ("", BrowserThemeProvider::AlignmentToString(alignment));
}

TEST(BrowserThemeProviderTest, AlignmentConversionInput) {
  // Verify that we output in an expected format.
  int alignment = BrowserThemeProvider::StringToAlignment("right bottom");
  EXPECT_EQ("bottom right", BrowserThemeProvider::AlignmentToString(alignment));

  // Verify that bad strings don't cause explosions.
  alignment = BrowserThemeProvider::StringToAlignment("new zealand");
  EXPECT_EQ("", BrowserThemeProvider::AlignmentToString(alignment));

  // Verify that bad strings don't cause explosions.
  alignment = BrowserThemeProvider::StringToAlignment("new zealand top");
  EXPECT_EQ("top", BrowserThemeProvider::AlignmentToString(alignment));

  // Verify that bad strings don't cause explosions.
  alignment = BrowserThemeProvider::StringToAlignment("new zealandtop");
  EXPECT_EQ("", BrowserThemeProvider::AlignmentToString(alignment));
}
