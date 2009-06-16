// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "app/gfx/font.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace {

using gfx::Font;

class FontTest : public testing::Test {
};

TEST_F(FontTest, LoadArial) {
  Font cf(Font::CreateFont(L"Arial", 16));
  ASSERT_TRUE(cf.nativeFont());
  ASSERT_EQ(cf.style(), Font::NORMAL);
  ASSERT_EQ(cf.FontSize(), 16);
  ASSERT_EQ(cf.FontName(), L"Arial");
}

TEST_F(FontTest, LoadArialBold) {
  Font cf(Font::CreateFont(L"Arial", 16));
  Font bold(cf.DeriveFont(0, Font::BOLD));
  ASSERT_TRUE(bold.nativeFont());
  ASSERT_EQ(bold.style(), Font::BOLD);
}

TEST_F(FontTest, Ascent) {
  Font cf(Font::CreateFont(L"Arial", 16));
  ASSERT_GT(cf.baseline(), 2);
  ASSERT_LT(cf.baseline(), 20);
}

TEST_F(FontTest, Height) {
  Font cf(Font::CreateFont(L"Arial", 16));
  ASSERT_GT(cf.baseline(), 2);
  ASSERT_LT(cf.baseline(), 20);
}

TEST_F(FontTest, AvgWidths) {
  Font cf(Font::CreateFont(L"Arial", 16));
  ASSERT_EQ(cf.GetExpectedTextWidth(0), 0);
  ASSERT_GT(cf.GetExpectedTextWidth(1), cf.GetExpectedTextWidth(0));
  ASSERT_GT(cf.GetExpectedTextWidth(2), cf.GetExpectedTextWidth(1));
  ASSERT_GT(cf.GetExpectedTextWidth(3), cf.GetExpectedTextWidth(2));
}

TEST_F(FontTest, Widths) {
  Font cf(Font::CreateFont(L"Arial", 16));
  ASSERT_EQ(cf.GetStringWidth(L""), 0);
  ASSERT_GT(cf.GetStringWidth(L"a"), cf.GetStringWidth(L""));
  ASSERT_GT(cf.GetStringWidth(L"ab"), cf.GetStringWidth(L"a"));
  ASSERT_GT(cf.GetStringWidth(L"abc"), cf.GetStringWidth(L"ab"));
}

#if defined(OS_WIN)
TEST_F(FontTest, DeriveFontResizesIfSizeTooSmall) {
  // This creates font of height -8.
  Font cf(Font::CreateFont(L"Arial", 6));
  Font derived_font = cf.DeriveFont(-4);
  LOGFONT font_info;
  GetObject(derived_font.hfont(), sizeof(LOGFONT), &font_info);
  EXPECT_EQ(-5, font_info.lfHeight);
}

TEST_F(FontTest, DeriveFontKeepsOriginalSizeIfHeightOk) {
  // This creates font of height -8.
  Font cf(Font::CreateFont(L"Arial", 6));
  Font derived_font = cf.DeriveFont(-2);
  LOGFONT font_info;
  GetObject(derived_font.hfont(), sizeof(LOGFONT), &font_info);
  EXPECT_EQ(-6, font_info.lfHeight);
}
#endif
}  // anonymous namespace
