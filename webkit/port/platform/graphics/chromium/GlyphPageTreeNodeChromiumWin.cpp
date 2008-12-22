// Copyright (c) 2008, Google Inc. All rights reserved.
// 
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
// 
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
// 
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "config.h"
#include <windows.h>
#include <vector>

#include "ChromiumBridge.h"
#include "ChromiumUtilsWin.h"
#include "Font.h"
#include "GlyphPageTreeNode.h"
#include "SimpleFontData.h"
#include "UniscribeHelperTextRun.h"

namespace WebCore
{

// Fills one page of font data pointers with NULL to indicate that there
// are no glyphs for the characters.
static void fillEmptyGlyphs(GlyphPage* page)
{
    for (int i = 0; i < GlyphPage::size; ++i)
        page->setGlyphDataForIndex(i, NULL, NULL);
}

// Lazily initializes space glyph
static Glyph initSpaceGlyph(HDC dc, Glyph* space_glyph)
{
    if (*space_glyph)
        return *space_glyph;
    static wchar_t space = ' ';
    GetGlyphIndices(dc, &space, 1, space_glyph, 0);
    return *space_glyph;
}

// Fills a page of glyphs in the Basic Multilingual Plane (<= U+FFFF). We
// can use the standard Windows GDI functions here. The input buffer size is
// assumed to be GlyphPage::size. Returns true if any glyphs were found.
static bool fillBMPGlyphs(UChar* buffer,
                          GlyphPage* page,
                          const SimpleFontData* fontData,
                          bool recurse)
{
    HDC dc = GetDC((HWND)0);
    HGDIOBJ old_font = SelectObject(dc, fontData->m_font.hfont());

    TEXTMETRIC tm = {0};
    if (!GetTextMetrics(dc, &tm)) {
        SelectObject(dc, old_font);
        ReleaseDC(0, dc);

        if (recurse) {
            if (ChromiumBridge::ensureFontLoaded(fontData->m_font.hfont())) {
                return fillBMPGlyphs(buffer, page, fontData, false);
            } else {
                fillEmptyGlyphs(page);
                return false;
            }
        } else {
            // TODO(nsylvain): This should never happen. We want to crash the
            // process and receive a crash dump. We should revisit this code later.
            // See bug 1136944.
            ASSERT_NOT_REACHED();
            fillEmptyGlyphs(page);
            return false;
        }
    }

    // NOTE(hbono): GetGlyphIndices() sets each item of localGlyphBuffer[]
    // with the one of the values listed below.
    //  * With the GGI_MARK_NONEXISTING_GLYPHS flag
    //    + If the font has a glyph available for the character,
    //      localGlyphBuffer[i] > 0x0.
    //    + If the font does not have glyphs available for the character,
    //      localGlyphBuffer[i] = 0x1F (TrueType Collection?) or
    //                            0xFFFF (OpenType?).
    //  * Without the GGI_MARK_NONEXISTING_GLYPHS flag
    //    + If the font has a glyph available for the character,
    //      localGlyphBuffer[i] > 0x0.
    //    + If the font does not have glyphs available for the character,
    //      localGlyphBuffer[i] = 0x80.
    //      (Windows automatically assigns the glyph for a box character to
    //      prevent ExtTextOut() from returning errors.)
    // To avoid from hurting the rendering performance, this code just
    // tells WebKit whether or not the all glyph indices for the given
    // characters are 0x80 (i.e. a possibly-invalid glyph) and let it
    // use alternative fonts for the characters.
    // Although this may cause a problem, it seems to work fine as far as I
    // have tested. (Obviously, I need more tests.)
    WORD localGlyphBuffer[GlyphPage::size];

    // NOTE(jnd). I find some Chinese characters can not be correctly displayed
    // when call GetGlyphIndices without flag GGI_MARK_NONEXISTING_GLYPHS,
    // because the corresponding glyph index is set as 0x20 when current font
    // does not have glyphs available for the character. According a blog post
    // http://blogs.msdn.com/michkap/archive/2006/06/28/649791.aspx
    // I think we should switch to the way about calling GetGlyphIndices with
    // flag GGI_MARK_NONEXISTING_GLYPHS, it should be OK according the
    // description of MSDN.
    // Also according to Jungshik and Hironori's suggestion and modification
    // we treat turetype and raster Font as different way when windows version
    // is less than Vista.
    GetGlyphIndices(dc, buffer, GlyphPage::size, localGlyphBuffer,
                    GGI_MARK_NONEXISTING_GLYPHS);

    // Copy the output to the GlyphPage
    bool have_glyphs = false;
    int invalid_glyph = 0xFFFF;
    if (!ChromiumUtils::isVistaOrGreater() && !(tm.tmPitchAndFamily & TMPF_TRUETYPE))
        invalid_glyph = 0x1F;

    Glyph space_glyph = 0;  // Glyph for a space. Lazily filled.

    for (unsigned i = 0; i < GlyphPage::size; i++) {
        UChar c = buffer[i];
        Glyph glyph = localGlyphBuffer[i];
        const SimpleFontData* glyphFontData = fontData;
        // When this character should be a space, we ignore whatever the font
        // says and use a space. Otherwise, if fonts don't map one of these
        // space or zero width glyphs, we will get a box.
        if (Font::treatAsSpace(c)) {
            // Hard code the glyph indices for characters that should be
            // treated like spaces.
            glyph = initSpaceGlyph(dc, &space_glyph);
        // TODO(dglazkov): change Font::treatAsZeroWidthSpace to use
        // u_hasBinaryProperty, per jungshik's comment here:
        // https://bugs.webkit.org/show_bug.cgi?id=20237#c6.
        // Then the additional OR won't be necessary.
        } else if (Font::treatAsZeroWidthSpace(c) || c == 0x200B) {
            glyph = initSpaceGlyph(dc, &space_glyph);
            glyphFontData = fontData->zeroWidthFontData();
        } else if (glyph == invalid_glyph) {
            // WebKit expects both the glyph index and FontData
            // pointer to be NULL if the glyph is not present
            glyph = 0;
            glyphFontData = 0;
        } else {
            if (SimpleFontData::isCJKCodePoint(c))
                glyphFontData = fontData->cjkWidthFontData();
            have_glyphs = true;
        }
        page->setGlyphDataForCharacter(i, glyph, glyphFontData);
    }

    SelectObject(dc, old_font);
    ReleaseDC(0, dc);
    return have_glyphs;
}

// For non-BMP characters, each is two words (UTF-16) and the input buffer size
// is (GlyphPage::size * 1). Since GDI doesn't know how to handle non-BMP
// characters, we must use Uniscribe to tell us the glyph indices.
//
// We don't want to call this in the case of "regular" characters since some
// fonts may not have the correct combining rules for accents. See the notes
// at the bottom of ScriptGetCMap. We can't use ScriptGetCMap, though, since
// it doesn't seem to support UTF-16, despite what this blog post says:
//   http://blogs.msdn.com/michkap/archive/2006/06/29/650680.aspx
//
// So we fire up the full Uniscribe doohicky, give it our string, and it will
// correctly handle the UTF-16 for us. The hard part is taking this and getting
// the glyph indices back out that correspond to the correct input characters,
// since they may be missing.
//
// Returns true if any glyphs were found.
static bool fillNonBMPGlyphs(UChar* buffer,
                             GlyphPage* page,
                             const SimpleFontData* fontData)
{
    bool haveGlyphs = false;

    UniscribeHelperTextRun state(buffer, GlyphPage::size * 2, false,
                                 fontData->m_font.hfont(),
                                 fontData->m_font.scriptCache(),
                                 fontData->m_font.scriptFontProperties());
    state.setInhibitLigate(true);
    state.Init();

    for (unsigned i = 0; i < GlyphPage::size; i++) {
        // Each character in this input buffer is a surrogate pair, which
        // consists of two UChars. So, the offset for its i-th character is
        // (i * 2).
        WORD glyph = state.FirstGlyphForCharacter(i * 2);
        if (glyph) {
            haveGlyphs = true;
            page->setGlyphDataForIndex(i, glyph, fontData);
        } else {
            // Clear both glyph and fontData fields.
            page->setGlyphDataForIndex(i, 0, 0);
        }
    }
    return haveGlyphs;
}

// We're supposed to return true if there are any glyphs in this page in our
// font, false if there are none.
bool GlyphPage::fill(unsigned offset, unsigned length, UChar* characterBuffer,
                     unsigned bufferLength, const SimpleFontData* fontData)
{
    // This function's parameters are kind of stupid. We always fill this page,
    // which is a fixed size. The source character indices are in the given
    // input buffer. For non-BMP characters each character will be represented
    // by a surrogate pair (two characters), so the input bufferLength will be
    // twice as big, even though the output size is the same.
    //
    // We have to handle BMP and non-BMP characters differently anyway...
    if (bufferLength == GlyphPage::size) {
        return fillBMPGlyphs(characterBuffer, this, fontData, true);
    } else if (bufferLength == GlyphPage::size * 2) {
        return fillNonBMPGlyphs(characterBuffer, this, fontData);
    } else {
        // TODO: http://b/1007391 make use of offset and length
        return false;
    }
}

}  // namespace WebCore
