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
    if (metrics.fVDMXMetricsValid) {
        m_ascent = metrics.fVDMXAscent;
        m_descent = metrics.fVDMXDescent;
    } else {
        m_ascent = SkScalarCeil(-metrics.fAscent);
        m_descent = SkScalarCeil(metrics.fHeight) - m_ascent;
    }

    GlyphPage* glyphPageZero = GlyphPageTreeNode::getRootChild(this, 0)->page();
    if (!glyphPageZero)
      return;

    static const UChar32 x_char = 'x';
    const Glyph x_glyph = glyphPageZero->glyphDataForCharacter(x_char).glyph;

    if (x_glyph) {
      // If the face includes a glyph for x we measure its height exactly.
      SkRect xbox;

      paint.setTextEncoding(SkPaint::kGlyphID_TextEncoding);
      paint.measureText(&x_glyph, 2, &xbox);

      m_xHeight = -xbox.fTop;
    } else {
      // hack taken from the Windows port
      m_xHeight = static_cast<float>(m_ascent) * 0.56;
    }

    m_lineGap = SkScalarRound(metrics.fLeading);
    m_lineSpacing = m_ascent + m_descent + m_lineGap;

    // In WebKit/WebCore/platform/graphics/SimpleFontData.cpp, m_spaceWidth is
    // calculated for us, but we need to calculate m_maxCharWidth and
    // m_avgCharWidth in order for text entry widgets to be sized correctly.

    m_maxCharWidth = SkScalarRound(metrics.fXRange * SkScalarRound(m_font.size()));

    if (metrics.fAvgCharWidth) {
        m_avgCharWidth = SkScalarRound(metrics.fAvgCharWidth);
    } else {
        if (x_glyph) {
          m_avgCharWidth = widthForGlyph(x_glyph);
        } else {
          m_avgCharWidth = m_xHeight;
        }
    }
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
