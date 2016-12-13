// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "UniscribeHelper.h"

#include "PlatformString.h"
#include "testing/gtest/include/gtest/gtest.h"

using WebCore::String;
using WebCore::UniscribeHelper;

namespace {

class UniscribeTest : public testing::Test {
public:
    UniscribeTest()
    {
    }

    // Returns an HFONT with the given name. The caller does not have to free
    // this, it will be automatically freed at the end of the test. Returns NULL
    // on failure. On success, the
    HFONT MakeFont(const wchar_t* fontName, SCRIPT_CACHE** cache)
    {
        LOGFONT lf;
        memset(&lf, 0, sizeof(LOGFONT));
        lf.lfHeight = 20;
        wcscpy_s(lf.lfFaceName, fontName);

        HFONT hfont = CreateFontIndirect(&lf);
        if (!hfont)
            return NULL;

        *cache = new SCRIPT_CACHE;
        **cache = NULL;
        m_createdFonts.append(std::make_pair(hfont, *cache));
        return hfont;
    }

protected:
    // Default font properties structure for tests to use.
    SCRIPT_FONTPROPERTIES m_properties;

private:
    virtual void SetUp()
    {
        memset(&m_properties, 0, sizeof(SCRIPT_FONTPROPERTIES));
        m_properties.cBytes = sizeof(SCRIPT_FONTPROPERTIES);
        m_properties.wgBlank = ' ';
        m_properties.wgDefault = '?';  // Used when the char is not in the font.
        m_properties.wgInvalid = '#';  // Used for invalid characters.
    }

    virtual void TearDown()
    {
        // Free any allocated fonts.
        for (size_t i = 0; i < m_createdFonts.size(); i++) {
            DeleteObject(m_createdFonts[i].first);
            ScriptFreeCache(m_createdFonts[i].second);
            delete m_createdFonts[i].second;
        }
        m_createdFonts.clear();
    }

    // Tracks allocated fonts so we can delete them at the end of the test.
    // The script cache pointer is heap allocated and must be freed.
    Vector< std::pair<HFONT, SCRIPT_CACHE*> > m_createdFonts;
};

}  // namespace

// This test tests giving Uniscribe a very large buffer, which will cause a
// failure.
TEST_F(UniscribeTest, TooBig)
{
    // Make a large string with an e with a zillion combining accents.
    String input(L"e");
    for (int i = 0; i < 100000; i++)
        input.append(static_cast<UChar>(0x301));  // Combining acute accent.

    SCRIPT_CACHE* scriptCache;
    HFONT hfont = MakeFont(L"Times New Roman", &scriptCache);
    ASSERT_TRUE(hfont);

    // Test a long string without the normal length protection we have. This
    // will cause shaping to fail.
    {
        UniscribeHelper uniscribe(
            input.characters(), static_cast<int>(input.length()),
            false, hfont, scriptCache, &m_properties);
        uniscribe.initWithOptionalLengthProtection(false);

        // There should be one shaping entry, with nothing in it.
        ASSERT_EQ(1, uniscribe.m_shapes.size());
        EXPECT_EQ(0, uniscribe.m_shapes[0].m_glyphs.size());
        EXPECT_EQ(0, uniscribe.m_shapes[0].m_logs.size());
        EXPECT_EQ(0, uniscribe.m_shapes[0].m_visualAttributes.size());
        EXPECT_EQ(0, uniscribe.m_shapes[0].m_advance.size());
        EXPECT_EQ(0, uniscribe.m_shapes[0].m_offsets.size());
        EXPECT_EQ(0, uniscribe.m_shapes[0].m_justify.size());
        EXPECT_EQ(0, uniscribe.m_shapes[0].m_abc.abcA);
        EXPECT_EQ(0, uniscribe.m_shapes[0].m_abc.abcB);
        EXPECT_EQ(0, uniscribe.m_shapes[0].m_abc.abcC);

        // The sizes of the other stuff should match the shaping entry.
        EXPECT_EQ(1, uniscribe.m_runs.size());
        EXPECT_EQ(1, uniscribe.m_screenOrder.size());

        // Check that the various querying functions handle the empty case
        // properly.
        EXPECT_EQ(0, uniscribe.width());
        EXPECT_EQ(0, uniscribe.firstGlyphForCharacter(0));
        EXPECT_EQ(0, uniscribe.firstGlyphForCharacter(1000));
        EXPECT_EQ(0, uniscribe.xToCharacter(0));
        EXPECT_EQ(0, uniscribe.xToCharacter(1000));
    }

    // Now test the very large string and make sure it is handled properly by
    // the length protection.
    {
        UniscribeHelper uniscribe(
            input.characters(), static_cast<int>(input.length()),
            false, hfont, scriptCache, &m_properties);
        uniscribe.initWithOptionalLengthProtection(true);

        // There should be 0 runs and shapes.
        EXPECT_EQ(0, uniscribe.m_runs.size());
        EXPECT_EQ(0, uniscribe.m_shapes.size());
        EXPECT_EQ(0, uniscribe.m_screenOrder.size());

        EXPECT_EQ(0, uniscribe.width());
        EXPECT_EQ(0, uniscribe.firstGlyphForCharacter(0));
        EXPECT_EQ(0, uniscribe.firstGlyphForCharacter(1000));
        EXPECT_EQ(0, uniscribe.xToCharacter(0));
        EXPECT_EQ(0, uniscribe.xToCharacter(1000));
    }
}
