// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "SimpleFontData.h"

#include "Font.h"
#include "FontCache.h"
#include "FloatRect.h"
#include "FontDescription.h"
#include "Logging.h"
#include "NotImplemented.h"

#include "SkPaint.h"
#include "SkTypeface.h"
#include "SkTime.h"

namespace WebCore {

// Smallcaps versions of fonts are 70% the size of the normal font.
static const float kSmallCapsFraction = 0.7f;

void SimpleFontData::platformInit()
{
    SkPaint paint;
    SkPaint::FontMetrics metrics;

    m_font.setupPaint(&paint);
    paint.getFontMetrics(&metrics);

    // Beware those who step here: This code is designed to match Win32 font
    // metrics *exactly*.
    m_ascent = SkScalarCeil(-metrics.fAscent);
    m_descent = SkScalarCeil(metrics.fDescent);
    m_xHeight = SkScalarToFloat(-metrics.fAscent) * 0.56f;   // hack I stole from the Windows port
    m_lineGap = SkScalarCeil(metrics.fLeading);
    m_lineSpacing = m_ascent + m_descent + m_lineGap;

    // In WebKit/WebCore/platform/graphics/SimpleFontData.cpp, m_spaceWidth is
    // calculated for us, but we need to calculate m_maxCharWidth and
    // m_avgCharWidth in order for text entry widgets to be sized correctly.
    // Skia doesn't expose either of these so we calculate them ourselves

    GlyphPage* glyphPageZero = GlyphPageTreeNode::getRootChild(this, 0)->page();
    if (!glyphPageZero)
      return;

    static const UChar32 e_char = 'e';
    static const UChar32 M_char = 'M';
    m_avgCharWidth = widthForGlyph(glyphPageZero->glyphDataForCharacter(e_char).glyph);
    m_maxCharWidth = widthForGlyph(glyphPageZero->glyphDataForCharacter(M_char).glyph);
}

void SimpleFontData::platformDestroy()
{
    delete m_smallCapsFontData;
}

SimpleFontData* SimpleFontData::smallCapsFontData(const FontDescription& fontDescription) const
{
    if (!m_smallCapsFontData) {
        m_smallCapsFontData =
            new SimpleFontData(FontPlatformData(m_font, fontDescription.computedSize() * kSmallCapsFraction));
    }
    return m_smallCapsFontData;
}

bool SimpleFontData::containsCharacters(const UChar* characters, int length) const
{
    SkPaint paint;
    static const unsigned kMaxBufferCount = 64;
    uint16_t glyphs[kMaxBufferCount];

    m_font.setupPaint(&paint);
    paint.setTextEncoding(SkPaint::kUTF16_TextEncoding);

    while (length > 0) {
        int n = SkMin32(length, SK_ARRAY_COUNT(glyphs));

        // textToGlyphs takes a byte count so we double the character count.
        int count = paint.textToGlyphs(characters, n * 2, glyphs);
        for (int i = 0; i < count; i++) {
            if (0 == glyphs[i]) {
                return false;       // missing glyph
            }
        }

        characters += n;
        length -= n;
    }
    return true;
}

void SimpleFontData::determinePitch()
{
    m_treatAsFixedPitch = platformData().isFixedPitch();
}

float SimpleFontData::platformWidthForGlyph(Glyph glyph) const
{
    SkASSERT(sizeof(glyph) == 2);   // compile-time assert

    SkPaint paint;

    m_font.setupPaint(&paint);

    paint.setTextEncoding(SkPaint::kGlyphID_TextEncoding);
    SkScalar width = paint.measureText(&glyph, 2);
    
    return SkScalarToFloat(width);
}

}  // namespace WebCore
