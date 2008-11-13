// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "Font.h"

#include "FloatRect.h"
#include "GlyphBuffer.h"
#include "GraphicsContext.h"
#include "NotImplemented.h"
#include "PlatformContextSkia.h"
#include "SimpleFontData.h"

#include "SkCanvas.h"
#include "SkPaint.h"
#include "SkTemplates.h"
#include "SkTypeface.h"
#include "SkUtils.h"

namespace WebCore {

void Font::drawGlyphs(GraphicsContext* gc, const SimpleFontData* font,
                      const GlyphBuffer& glyphBuffer,  int from, int numGlyphs,
                      const FloatPoint& point) const {
    SkCanvas* canvas = gc->platformContext()->canvas();
    SkPaint paint;

    font->platformData().setupPaint(&paint);
    paint.setTextEncoding(SkPaint::kGlyphID_TextEncoding);
    paint.setColor(gc->fillColor().rgb());

    SkASSERT(sizeof(GlyphBufferGlyph) == sizeof(uint16_t));  // compile-time assert

    const GlyphBufferGlyph* glyphs = glyphBuffer.glyphs(from);
    SkScalar x = SkFloatToScalar(point.x());
    SkScalar y = SkFloatToScalar(point.y());

    // TODO(port): Android WebCore has patches for PLATFORM(SGL) which involves
    // this, however we don't have these patches and it's unclear when Android
    // may upstream them.
#if 0
    if (glyphBuffer.hasAdjustedWidths()) {
        const GlyphBufferAdvance*   adv = glyphBuffer.advances(from);
        SkAutoSTMalloc<32, SkPoint> storage(numGlyphs);
        SkPoint*                    pos = storage.get();

        for (int i = 0; i < numGlyphs; i++) {
            pos[i].set(x, y);
            x += SkFloatToScalar(adv[i].width());
            y += SkFloatToScalar(adv[i].height());
        }
        canvas->drawPosText(glyphs, numGlyphs << 1, pos, paint);
    } else {
        canvas->drawText(glyphs, numGlyphs << 1, x, y, paint);
    }
#endif

    canvas->drawText(glyphs, numGlyphs << 1, x, y, paint);
}

void Font::drawComplexText(GraphicsContext* context, const TextRun& run,
                           const FloatPoint& point, int from, int to) const
{
    notImplemented();
}

float Font::floatWidthForComplexText(const TextRun& run) const
{
    notImplemented();
    return 0;
}

int Font::offsetForPositionForComplexText(const TextRun& run, int x,
                                          bool includePartialGlyphs) const
{
    notImplemented();
    return 0;
}

FloatRect Font::selectionRectForComplexText(const TextRun& run,
                                            const IntPoint& point, int h,
                                            int from, int to) const
{
    notImplemented();
    return FloatRect();
}

}  // namespace WebCore
