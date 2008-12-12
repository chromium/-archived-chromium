// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/gfx/chrome_font.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace {

class ChromeFontTest : public testing::Test {
};

TEST_F(ChromeFontTest, LoadArial) {
  ChromeFont cf(ChromeFont::CreateFont(L"Arial", 16));
  ASSERT_TRUE(cf.nativeFont());
  ASSERT_EQ(cf.style(), ChromeFont::NORMAL);
  ASSERT_EQ(cf.FontSize(), 16);
  ASSERT_EQ(cf.FontName(), L"Arial");
}

TEST_F(ChromeFontTest, LoadArialBold) {
  ChromeFont cf(ChromeFont::CreateFont(L"Arial", 16));
  ChromeFont bold(cf.DeriveFont(0, ChromeFont::BOLD));
  ASSERT_TRUE(bold.nativeFont());
  ASSERT_EQ(bold.style(), ChromeFont::BOLD);
}

TEST_F(ChromeFontTest, Ascent) {
  ChromeFont cf(ChromeFont::CreateFont(L"Arial", 16));
  ASSERT_GT(cf.baseline(), 2);
  ASSERT_LT(cf.baseline(), 20);
}

TEST_F(ChromeFontTest, Height) {
  ChromeFont cf(ChromeFont::CreateFont(L"Arial", 16));
  ASSERT_GT(cf.baseline(), 2);
  ASSERT_LT(cf.baseline(), 20);
}

TEST_F(ChromeFontTest, AvgWidths) {
  ChromeFont cf(ChromeFont::CreateFont(L"Arial", 16));
  ASSERT_EQ(cf.GetExpectedTextWidth(0), 0);
  ASSERT_GT(cf.GetExpectedTextWidth(1), cf.GetExpectedTextWidth(0));
  ASSERT_GT(cf.GetExpectedTextWidth(2), cf.GetExpectedTextWidth(1));
  ASSERT_GT(cf.GetExpectedTextWidth(3), cf.GetExpectedTextWidth(2));
}

TEST_F(ChromeFontTest, Widths) {
  ChromeFont cf(ChromeFont::CreateFont(L"Arial", 16));
  ASSERT_EQ(cf.GetStringWidth(L""), 0);
  ASSERT_GT(cf.GetStringWidth(L"a"), cf.GetStringWidth(L""));
  ASSERT_GT(cf.GetStringWidth(L"ab"), cf.GetStringWidth(L"a"));
  ASSERT_GT(cf.GetStringWidth(L"abc"), cf.GetStringWidth(L"ab"));
}

}  // anonymous namespace
