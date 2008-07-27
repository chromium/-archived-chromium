/*
 * Copyright (C) 2006, 2007 Apple Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"
#include <windows.h>

#include "Font.h"
#include "FontFallbackList.h"
#include "GlyphBuffer.h"
#include "SimpleFontData.h"
#include "UniscribeStateTextRun.h"

#include "base/gfx/platform_canvas.h"
#include "graphics/SkiaUtils.h"
#include "webkit/glue/webkit_glue.h"


namespace WebCore {

void Font::drawGlyphs(GraphicsContext* graphicsContext,
                      const SimpleFontData* font,
                      const GlyphBuffer& glyphBuffer,
                      int from,
                      int numGlyphs,
                      const FloatPoint& point) const
{
    PlatformGraphicsContext* context = graphicsContext->platformContext();
    SkPoint origin;
    WebCorePointToSkiaPoint(point, &origin);

    // Max buffer length passed to the underlying windows API.
    const int kMaxBufferLength = 1024;
    // Default size for the buffer. It should be enough for most of cases.
    const int kDefaultBufferLength = 256;

    // Windows needs the characters and the advances in nice contiguous
    // buffers, which we build here.
    Vector<WORD, kDefaultBufferLength> glyphs;
    Vector<int, kDefaultBufferLength> advances;

    // We draw the glyphs in chunks to avoid having to do a heap allocation for
    // the arrays of characters and advances. Since ExtTextOut is the
    // lowest-level text output function on Windows, there should be little
    // penalty for splitting up the text. On the other hand, the buffer cannot
    // be bigger than 4094 or the function will fail.
    int glyph_index = 0;
    int chunk_x = 0;  // x offset of this span from the point
    while (glyph_index < numGlyphs) {
      // how many chars will be in this chunk?
      int cur_len = std::min(kMaxBufferLength, numGlyphs - glyph_index);

      glyphs.resize(cur_len);
      advances.resize(cur_len);

      int cur_width = 0;
      for (int i = 0; i < cur_len; ++i, ++glyph_index) {
        glyphs[i] = glyphBuffer.glyphAt(from + glyph_index);
        advances[i] = static_cast<int>(glyphBuffer.advanceAt(from +
                                                             glyph_index));
        cur_width += advances[i];
      }

      // the 'point' represents the baseline, so we need to move it up to the
      // top of the bounding square by subtracting the ascent
      SkPoint origin2 = origin;
      origin2.fY -= font->ascent();
      origin2.fX += chunk_x;

      bool success = false;
      for (int executions = 0; executions < 2; ++executions) {
        success = context->paintText(font->platformData().hfont(),
                                     cur_len,
                                     &glyphs[0],
                                     &advances[0],
                                     origin2);
        if (!success && executions == 0) {
          // Ask the browser to load the font for us and retry.
          webkit_glue::EnsureFontLoaded(font->platformData().hfont());
          continue;
        }
        break;
      }

      ASSERT(success);

      chunk_x += cur_width;
    }
}

FloatRect Font::selectionRectForComplexText(const TextRun& run,
                                            const IntPoint& point,
                                            int h,
                                            int from,
                                            int to) const
{
    UniscribeStateTextRun state(run, *this);
    float left = static_cast<float>(point.x() + state.CharacterToX(from));
    float right = static_cast<float>(point.x() + state.CharacterToX(to));

    // If the text is RTL, left will actually be after right.
    if (left < right) {
      return FloatRect(left, static_cast<float>(point.y()),
                       right - left, static_cast<float>(h));
    }
    return FloatRect(right, static_cast<float>(point.y()),
                     left - right, static_cast<float>(h));
}

void Font::drawComplexText(GraphicsContext* context,
                           const TextRun& run,
                           const FloatPoint& point,
                           int from,
                           int to) const
{
    UniscribeStateTextRun state(run, *this);
    PlatformContextSkia* skia = PlatformContextToPlatformContextSkia(context->platformContext());
    SkPoint p;
    WebCorePointToSkiaPoint(point, &p);
    skia->paintComplexText(state, p, from, to, ascent());
}

float Font::floatWidthForComplexText(const TextRun& run) const
{
    UniscribeStateTextRun state(run, *this);
    return static_cast<float>(state.Width());
}

int Font::offsetForPositionForComplexText(const TextRun& run, int x, bool includePartialGlyphs) const
{
    // Mac code ignores includePartialGlyphs, and they don't know what it's
    // supposed to do, so we just ignore it as well.
    UniscribeStateTextRun state(run, *this);
    int char_index = state.XToCharacter(x);

    // XToCharacter will return -1 if the position is before the first
    // character (we get called like this sometimes).
    if (char_index < 0)
      char_index = 0;
    return char_index;
}

}
