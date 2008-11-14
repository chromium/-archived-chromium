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

#include "ChromiumBridge.h"
#include "Font.h"
#include "FontFallbackList.h"
#include "GlyphBuffer.h"
#include "PlatformContextSkia.h"
#include "SimpleFontData.h"
#include "UniscribeStateTextRun.h"

#include "base/gfx/platform_canvas_win.h"
#include "base/gfx/skia_utils.h"
#include "graphics/SkiaUtils.h"


namespace WebCore {

void Font::drawGlyphs(GraphicsContext* graphicsContext,
                      const SimpleFontData* font,
                      const GlyphBuffer& glyphBuffer,
                      int from,
                      int numGlyphs,
                      const FloatPoint& point) const
{
    PlatformGraphicsContext* context = graphicsContext->platformContext();

    // Max buffer length passed to the underlying windows API.
    const int kMaxBufferLength = 1024;
    // Default size for the buffer. It should be enough for most of cases.
    const int kDefaultBufferLength = 256;

    SkColor color = context->fillColor();
    unsigned char alpha = SkColorGetA(color);
    // Skip 100% transparent text; no need to draw anything.
    if (!alpha)
        return;

    // Set up our graphics context.
    HDC hdc = context->canvas()->beginPlatformPaint();
    HGDIOBJ oldFont = SelectObject(hdc, font->platformData().hfont());

    // TODO(maruel): http://b/700464 SetTextColor doesn't support transparency.
    // Enforce non-transparent color.
    color = SkColorSetRGB(SkColorGetR(color),
                          SkColorGetG(color),
                          SkColorGetB(color));
    SetTextColor(hdc, gfx::SkColorToCOLORREF(color));
    SetBkMode(hdc, TRANSPARENT);

    // Windows needs the characters and the advances in nice contiguous
    // buffers, which we build here.
    Vector<WORD, kDefaultBufferLength> glyphs;
    Vector<int, kDefaultBufferLength> advances;

    // Compute the coordinate. The 'origin' represents the baseline, so we need
    // to move it up to the top of the bounding square.
    int x = static_cast<int>(point.x());
    int lineTop = static_cast<int>(point.y()) - font->ascent();

    // We draw the glyphs in chunks to avoid having to do a heap allocation for
    // the arrays of characters and advances. Since ExtTextOut is the
    // lowest-level text output function on Windows, there should be little
    // penalty for splitting up the text. On the other hand, the buffer cannot
    // be bigger than 4094 or the function will fail.
    int glyphIndex = 0;
    while (glyphIndex < numGlyphs) {
        // how many chars will be in this chunk?
        int curLen = std::min(kMaxBufferLength, numGlyphs - glyphIndex);

        glyphs.resize(curLen);
        advances.resize(curLen);

        int curWidth = 0;
        for (int i = 0; i < curLen; ++i, ++glyphIndex) {
            glyphs[i] = glyphBuffer.glyphAt(from + glyphIndex);
            advances[i] =
                static_cast<int>(glyphBuffer.advanceAt(from + glyphIndex));
            curWidth += advances[i];
        }

        bool success = false;
        for (int executions = 0; executions < 2; ++executions) {
            success = !!ExtTextOut(hdc, x, lineTop, ETO_GLYPH_INDEX, NULL,
                                   reinterpret_cast<const wchar_t*>(&glyphs[0]),
                                   curLen,
                                   &advances[0]);

            if (!success && executions == 0) {
                // Ask the browser to load the font for us and retry.
                ChromiumBridge::ensureFontLoaded(font->platformData().hfont());
                continue;
            }
            break;
        }

        ASSERT(success);

        x += curWidth;
    }

    SelectObject(hdc, oldFont);
    context->canvas()->endPlatformPaint();
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

void Font::drawComplexText(GraphicsContext* graphicsContext,
                           const TextRun& run,
                           const FloatPoint& point,
                           int from,
                           int to) const
{
    PlatformGraphicsContext* context = graphicsContext->platformContext();
    UniscribeStateTextRun state(run, *this);

    SkColor color = context->fillColor();
    uint8 alpha = SkColorGetA(color);
    // Skip 100% transparent text; no need to draw anything.
    if (!alpha)
        return;

    HDC hdc = context->canvas()->beginPlatformPaint();

    // TODO(maruel): http://b/700464 SetTextColor doesn't support transparency.
    // Enforce non-transparent color.
    color = SkColorSetRGB(SkColorGetR(color),
                          SkColorGetG(color),
                          SkColorGetB(color));
    SetTextColor(hdc, gfx::SkColorToCOLORREF(color));
    SetBkMode(hdc, TRANSPARENT);

    // Uniscribe counts the coordinates from the upper left, while WebKit uses
    // the baseline, so we have to subtract off the ascent.
    state.Draw(hdc,
               static_cast<int>(point.x()),
               static_cast<int>(point.y() - ascent()),
               from,
               to);
    context->canvas()->endPlatformPaint();
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

} // namespace WebCore
