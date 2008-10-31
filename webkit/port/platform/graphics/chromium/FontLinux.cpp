// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "Font.h"

#include <pango/pango.h>
#include <pango/pangoft2.h>

#include "FloatRect.h"
#include "NotImplemented.h"
#include "PlatformContextSkia.h"
#include "GraphicsContext.h"
#include "SimpleFontData.h"
#include "GlyphBuffer.h"

namespace WebCore {

// -----------------------------------------------------------------------------
// Bitblit a Freetype bitmap onto a canvas at the given location in the given
// colour.
//    pgc: the Skia canvas
//    bm: A freetype bitmap which is an 8-bit alpha bitmap
// -----------------------------------------------------------------------------
static void bitBlitAlpha(PlatformGraphicsContext* pgc, FT_Bitmap* bm,
                         int x, int y, const Color& col)
{
    SkPaint paint;
    paint.setARGB(col.alpha(), col.red(), col.green(), col.blue());

    // Here we are constructing an SkBitmap by pointing directly into the
    // Freetype bitmap data
    SkBitmap glyph;
    glyph.setConfig(SkBitmap::kA8_Config, bm->width, bm->rows, bm->pitch);
    glyph.setPixels(bm->buffer);
    pgc->canvas()->drawBitmap(glyph, x, y, &paint);
}

void Font::drawGlyphs(GraphicsContext* ctx, const SimpleFontData* sfd,
                      const GlyphBuffer& glyphBuffer, int from, int to,
                      const FloatPoint& point) const
{
    // For now we draw text by getting the Freetype face from Pango and asking
    // Freetype to render each glyph as an 8-bit alpha bitmap and drawing that
    // to the canvas. This, obviously, ignores kerning, ligatures and other
    // things that we should have in the real version.
    GlyphBufferGlyph* glyphs = (GlyphBufferGlyph*)glyphBuffer.glyphs(from);
    PlatformGraphicsContext* pgc = ctx->platformContext();
    FT_Face face = pango_ft2_font_get_face(sfd->m_font.m_font);
    FT_GlyphSlot slot = face->glyph;

    int x = point.x(), y = point.y();

    for (int i = from; i < to; ++i) {
        const FT_Error error = FT_Load_Glyph(face, glyphs[i], FT_LOAD_RENDER);
        if (error)
            continue;

        bitBlitAlpha(pgc, &slot->bitmap, x + slot->bitmap_left,
                     y - slot->bitmap_top, ctx->fillColor());
        // Freetype works in 1/64ths of a pixel, so we divide by 64 to get the
        // number of pixels to advance.
        x += slot->advance.x >> 6;
    }
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
