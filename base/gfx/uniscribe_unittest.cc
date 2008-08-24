// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/gfx/uniscribe.h"
#include "base/win_util.h"
#include "testing/gtest/include/gtest/gtest.h"

// This must be in the gfx namespace for the friend statements in uniscribe.h
// to work.
namespace gfx {

namespace {

class UniscribeTest : public testing::Test {
 public:
  UniscribeTest() {
  }

  // Returns an HFONT with the given name. The caller does not have to free
  // this, it will be automatically freed at the end of the test. Returns NULL
  // on failure. On success, the
  HFONT MakeFont(const wchar_t* font_name, SCRIPT_CACHE** cache) {
    LOGFONT lf;
    memset(&lf, 0, sizeof(LOGFONT));
    lf.lfHeight = 20;
    wcscpy_s(lf.lfFaceName, font_name);

    HFONT hfont = CreateFontIndirect(&lf);
    if (!hfont)
      return NULL;

    *cache = new SCRIPT_CACHE;
    **cache = NULL;
    created_fonts_.push_back(std::make_pair(hfont, *cache));
    return hfont;
  }

 protected:
  // Default font properties structure for tests to use.
  SCRIPT_FONTPROPERTIES properties_;

 private:
  virtual void SetUp() {
    memset(&properties_, 0, sizeof(SCRIPT_FONTPROPERTIES));
    properties_.cBytes = sizeof(SCRIPT_FONTPROPERTIES);
    properties_.wgBlank = ' ';
    properties_.wgDefault = '?';  // Used when the character is not in the font.
    properties_.wgInvalid = '#';  // Used for invalid characters.
  }

  virtual void TearDown() {
    // Free any allocated fonts.
    for (size_t i = 0; i < created_fonts_.size(); i++) {
      DeleteObject(created_fonts_[i].first);
      ScriptFreeCache(created_fonts_[i].second);
      delete created_fonts_[i].second;
    }
    created_fonts_.clear();
  }

  // Tracks allocated fonts so we can delete them at the end of the test.
  // The script cache pointer is heap allocated and must be freed.
  std::vector< std::pair<HFONT, SCRIPT_CACHE*> > created_fonts_;

  DISALLOW_EVIL_CONSTRUCTORS(UniscribeTest);
};

}  // namespace

// This test tests giving Uniscribe a very large buffer, which will cause a
// failure.
TEST_F(UniscribeTest, TooBig) {
  // This test will only run on Windows XP. It seems Uniscribe does not have the
  // internal limit on Windows 2000 that we rely on to cause this failure.
  if (win_util::GetWinVersion() <= win_util::WINVERSION_2000)
    return;

  // Make a large string with an e with a zillion combining accents.
  std::wstring input(L"e");
  for (int i = 0; i < 100000; i++)
    input.push_back(0x301);  // Combining acute accent.

  SCRIPT_CACHE* script_cache;
  HFONT hfont = MakeFont(L"Times New Roman", &script_cache);
  ASSERT_TRUE(hfont);

  // Test a long string without the normal length protection we have. This will
  // cause shaping to fail.
  {
    gfx::UniscribeState uniscribe(input.data(), static_cast<int>(input.size()),
                                  false, hfont, script_cache, &properties_);
    uniscribe.InitWithOptionalLengthProtection(false);

    // There should be one shaping entry, with nothing in it.
    ASSERT_EQ(1, uniscribe.shapes_->size());
    EXPECT_EQ(0, uniscribe.shapes_[0].glyphs->size());
    EXPECT_EQ(0, uniscribe.shapes_[0].logs->size());
    EXPECT_EQ(0, uniscribe.shapes_[0].visattr->size());
    EXPECT_EQ(0, uniscribe.shapes_[0].advance->size());
    EXPECT_EQ(0, uniscribe.shapes_[0].offsets->size());
    EXPECT_EQ(0, uniscribe.shapes_[0].justify->size());
    EXPECT_EQ(0, uniscribe.shapes_[0].abc.abcA);
    EXPECT_EQ(0, uniscribe.shapes_[0].abc.abcB);
    EXPECT_EQ(0, uniscribe.shapes_[0].abc.abcC);

    // The sizes of the other stuff should match the shaping entry.
    EXPECT_EQ(1, uniscribe.runs_->size());
    EXPECT_EQ(1, uniscribe.screen_order_->size());

    // Check that the various querying functions handle the empty case properly.
    EXPECT_EQ(0, uniscribe.Width());
    EXPECT_EQ(0, uniscribe.FirstGlyphForCharacter(0));
    EXPECT_EQ(0, uniscribe.FirstGlyphForCharacter(1000));
    EXPECT_EQ(0, uniscribe.XToCharacter(0));
    EXPECT_EQ(0, uniscribe.XToCharacter(1000));
  }

  // Now test the very large string and make sure it is handled properly by the
  // length protection.
  {
    gfx::UniscribeState uniscribe(input.data(), static_cast<int>(input.size()),
                                  false, hfont, script_cache, &properties_);
    uniscribe.InitWithOptionalLengthProtection(true);

    // There should be 0 runs and shapes.
    EXPECT_EQ(0, uniscribe.runs_->size());
    EXPECT_EQ(0, uniscribe.shapes_->size());
    EXPECT_EQ(0, uniscribe.screen_order_->size());

    EXPECT_EQ(0, uniscribe.Width());
    EXPECT_EQ(0, uniscribe.FirstGlyphForCharacter(0));
    EXPECT_EQ(0, uniscribe.FirstGlyphForCharacter(1000));
    EXPECT_EQ(0, uniscribe.XToCharacter(0));
    EXPECT_EQ(0, uniscribe.XToCharacter(1000));
  }
}

}  // namespace gfx

