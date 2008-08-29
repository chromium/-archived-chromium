/*
 * Copyright (C) 2005, 2008 Apple Computer, Inc.  All rights reserved.
 * Copyright (C) 2006 Alexey Proskuryakov
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer. 
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution. 
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "SimpleFontData.h"

#include "Font.h"

#if ENABLE(SVG_FONTS)
#include "SVGFontData.h"
#include "SVGFontFaceElement.h"
#endif

#include <wtf/MathExtras.h>

namespace WebCore {

SimpleFontData::SimpleFontData(const FontPlatformData& f, bool customFont, bool loading, SVGFontData* svgFontData)
    : m_font(f)
    , m_treatAsFixedPitch(false)
#if ENABLE(SVG_FONTS)
    , m_svgFontData(svgFontData)
#endif
    , m_isCustomFont(customFont)
    , m_isLoading(loading)
    , m_smallCapsFontData(0)
    , m_zeroWidthFontData(new ZeroWidthFontData())
    , m_cjkWidthFontData(new CJKWidthFontData())
{
#if ENABLE(SVG_FONTS) && !PLATFORM(QT)
    if (SVGFontFaceElement* svgFontFaceElement = svgFontData ? svgFontData->svgFontFaceElement() : 0) {
       m_unitsPerEm = svgFontFaceElement->unitsPerEm();

       double scale = f.size();
       if (m_unitsPerEm)
           scale /= m_unitsPerEm;

        m_ascent = static_cast<int>(svgFontFaceElement->ascent() * scale);
        m_descent = static_cast<int>(svgFontFaceElement->descent() * scale);
        // TODO(ojan): What should SVG fonts use for avgCharWidth/maxCharWidth?
        // This is currently only used in RenderTextControl.cpp for the width
        // of textareas with a non-fixed width.
        m_avgCharWidth = 0;
        m_maxCharWidth = 0;
        m_xHeight = static_cast<int>(svgFontFaceElement->xHeight() * scale);
        m_lineGap = 0.1f * f.size();
        m_lineSpacing = m_ascent + m_descent + m_lineGap;
    
        m_spaceGlyph = 0;
        m_spaceWidth = 0;
        m_adjustedSpaceWidth = 0;
        determinePitch();
        m_missingGlyphData.fontData = this;
        m_missingGlyphData.glyph = 0;
        m_zeroWidthFontData->init(this);
        m_cjkWidthFontData->init(this);
        return;
    }
#endif

    platformInit();

    GlyphPage* glyphPageZero = GlyphPageTreeNode::getRootChild(this, 0)->page();
    if (!glyphPageZero) {
        LOG_ERROR("Failed to get glyph page zero.");
        m_spaceGlyph = 0;
        m_spaceWidth = 0;
        m_adjustedSpaceWidth = 0;
        determinePitch();
        m_missingGlyphData.fontData = this;
        m_missingGlyphData.glyph = 0;
        m_zeroWidthFontData->init(this);
        m_cjkWidthFontData->init(this);
        return;
    }

    // Nasty hack to determine if we should round or ceil space widths.
    // If the font is monospace or fake monospace we ceil to ensure that 
    // every character and the space are the same width.  Otherwise we round.
    static const UChar32 space_char = ' ';
    m_spaceGlyph = glyphPageZero->glyphDataForCharacter(space_char).glyph;
    float width = widthForGlyph(m_spaceGlyph);
    m_spaceWidth = width;
    determinePitch();
    m_adjustedSpaceWidth = m_treatAsFixedPitch ? ceilf(width) : roundf(width);

    // TODO(dglazkov): Investigate and implement across platforms, if needed
#if PLATFORM(WIN)
    // ZERO WIDTH SPACES are explicitly mapped to share the glyph
    // with SPACE (with width adjusted to 0) during GlyphPage::fill
    // This is currently only implemented for Windows port. The FontData
    // remapping may very well be needed for other platforms.
#else
    // Force the glyph for ZERO WIDTH SPACE to have zero width, unless it is shared with SPACE.
    // Helvetica is an example of a non-zero width ZERO WIDTH SPACE glyph.
    // See <http://bugs.webkit.org/show_bug.cgi?id=13178>
    // Ask for the glyph for 0 to avoid paging in ZERO WIDTH SPACE. Control characters, including 0,
    // are mapped to the ZERO WIDTH SPACE glyph.
    Glyph zeroWidthSpaceGlyph = glyphPageZero->glyphDataForCharacter(0).glyph;
    if (zeroWidthSpaceGlyph) {
        if (zeroWidthSpaceGlyph != m_spaceGlyph)
            m_glyphToWidthMap.setWidthForGlyph(zeroWidthSpaceGlyph, 0);
        else
            LOG_ERROR("Font maps SPACE and ZERO WIDTH SPACE to the same glyph. Glyph width not overridden.");
    }
#endif

    m_missingGlyphData.fontData = this;
    m_missingGlyphData.glyph = 0;
    m_zeroWidthFontData->init(this);
    m_cjkWidthFontData->init(this);
}

SimpleFontData::SimpleFontData()
    : m_treatAsFixedPitch(false)
#if ENABLE(SVG_FONTS)
    , m_svgFontData(0)
#endif
    , m_isCustomFont(0)
    , m_isLoading(0)
    , m_smallCapsFontData(0)
{
}

SimpleFontData::~SimpleFontData()
{
#if ENABLE(SVG_FONTS) && !PLATFORM(QT)
    if (!m_svgFontData || !m_svgFontData->svgFontFaceElement())
#endif
        platformDestroy();

    // We only get deleted when the cache gets cleared.  Since the smallCapsRenderer is also in that cache,
    // it will be deleted then, so we don't need to do anything here.
}

float SimpleFontData::widthForGlyph(Glyph glyph) const
{
    float width = m_glyphToWidthMap.widthForGlyph(glyph);
    if (width != cGlyphWidthUnknown)
        return width;

    width = platformWidthForGlyph(glyph);
    m_glyphToWidthMap.setWidthForGlyph(glyph, width);

    return width;
}

const SimpleFontData* SimpleFontData::fontDataForCharacter(UChar32) const
{
    return this;
}

bool SimpleFontData::isSegmented() const
{
    return false;
}

const SimpleFontData* SimpleFontData::zeroWidthFontData() const
{
    return m_zeroWidthFontData.get();
}

const SimpleFontData* SimpleFontData::cjkWidthFontData() const
{
    return m_cjkWidthFontData.get();
}

void ZeroWidthFontData::init(SimpleFontData* fontData)
{
    m_font = fontData->m_font;
    m_smallCapsFontData = fontData->m_smallCapsFontData;
    m_ascent = fontData->m_ascent;
    m_descent = fontData->m_descent;
    m_lineSpacing = fontData->m_lineSpacing;
    m_lineGap = fontData->m_lineGap;
    m_maxCharWidth = 0;
    m_avgCharWidth = 0;
    m_xHeight = fontData->m_xHeight;
    m_unitsPerEm = fontData->m_unitsPerEm;
    m_spaceWidth = 0;
    m_spaceGlyph = 0;
    m_adjustedSpaceWidth = fontData->m_adjustedSpaceWidth;
#if PLATFORM(WIN)
    m_scriptCache = 0;
    m_scriptFontProperties = 0;
#endif
}

CJKWidthFontData::CJKWidthFontData()
    : m_cjkGlyphWidth(cGlyphWidthUnknown)
{
}

float CJKWidthFontData::widthForGlyph(Glyph glyph) const
{
    if (m_cjkGlyphWidth != cGlyphWidthUnknown)
        return m_cjkGlyphWidth;

    float width = platformWidthForGlyph(glyph);
    m_cjkGlyphWidth = width;

#ifndef NDEBUG
    // Test our optimization that assuming all CGK glyphs have the same width
    const float actual_width = platformWidthForGlyph(glyph);
    ASSERT((cGlyphWidthUnknown == width) || (actual_width == width));
#endif

    return width;
}

// static
// TODO(dglazkov): Move to Font::isCJKCodePoint for consistency
bool SimpleFontData::isCJKCodePoint(UChar32 c)
{
    // AC00..D7AF; Hangul Syllables
    if ((0xAC00 <= c) && (c <= 0xD7AF))
        return true;

    // CJK ideographs
    UErrorCode errorCode;
    return uscript_getScript(c, &errorCode) == USCRIPT_HAN &&
        U_SUCCESS(errorCode);
}


} // namespace WebCore
