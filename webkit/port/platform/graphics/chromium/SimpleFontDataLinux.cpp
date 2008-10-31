// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#define PANGO_ENABLE_BACKEND

#include "config.h"
#include "SimpleFontData.h"

#include <pango/pango.h>
#include <pango/pangoft2.h>
#include <pango/pangofc-font.h>

#include "Font.h"
#include "FontCache.h"
#include "FloatRect.h"
#include "FontDescription.h"
#include "Logging.h"
#include "NotImplemented.h"

namespace WebCore {

// TODO(agl): only stubs

void SimpleFontData::platformInit()
{
    PangoFont *const font = platformData().m_font;

    PangoFontMetrics *const metrics = pango_font_get_metrics(font, NULL);
    m_ascent = pango_font_metrics_get_ascent(metrics) / PANGO_SCALE;
    m_descent = pango_font_metrics_get_descent(metrics) / PANGO_SCALE;
    m_lineSpacing = m_ascent + m_descent;
    m_avgCharWidth = pango_font_metrics_get_approximate_char_width(metrics) / PANGO_SCALE;
    pango_font_metrics_unref(metrics);

    const guint xglyph = pango_fc_font_get_glyph(PANGO_FC_FONT(font), 'x');
    const guint spaceglyph = pango_fc_font_get_glyph(PANGO_FC_FONT(font), ' ');
    PangoRectangle rect;

    pango_font_get_glyph_extents(font, xglyph, &rect, NULL);
    m_xHeight = rect.height / PANGO_SCALE;
    pango_font_get_glyph_extents(font, spaceglyph, NULL, &rect);
    m_spaceWidth = rect.width / PANGO_SCALE;
    m_lineGap = m_lineSpacing - m_ascent - m_descent;

    FT_Face face = pango_ft2_font_get_face(font);
    m_unitsPerEm = face->units_per_EM / PANGO_SCALE;

    // TODO(agl): I'm not sure we have good data for this so it's 0 for now
    m_maxCharWidth = 0;
}

void SimpleFontData::platformDestroy() { }

SimpleFontData* SimpleFontData::smallCapsFontData(const FontDescription& fontDescription) const
{
    notImplemented();
    return NULL;
}

bool SimpleFontData::containsCharacters(const UChar* characters,
                                        int length) const
{
    bool result = true;

    PangoCoverage* requested = pango_coverage_from_bytes((guchar*)characters, length);
    PangoCoverage* available = pango_font_get_coverage(m_font.m_font, pango_language_get_default());
    pango_coverage_max(requested, available);

    for (int i = 0; i < length; i++) {
        if (PANGO_COVERAGE_NONE == pango_coverage_get(requested, i)) {
            result = false;
            break;
        }
    }

    pango_coverage_unref(requested);
    pango_coverage_unref(available);

    return result;
}

void SimpleFontData::determinePitch()
{
    m_treatAsFixedPitch = platformData().isFixedPitch();
}

float SimpleFontData::platformWidthForGlyph(Glyph glyph) const
{
    PangoFont *const font = platformData().m_font;
    PangoRectangle rect;

    pango_font_get_glyph_extents(font, glyph, NULL, &rect);

    return static_cast<float>(rect.width) / PANGO_SCALE;
}

}  // namespace WebCore
