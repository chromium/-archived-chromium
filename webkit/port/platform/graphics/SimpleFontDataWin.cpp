/*
 * Copyright (C) 2006, 2007 Apple Inc.  All rights reserved.
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
#include "Font.h"
#include "FontCache.h"
#include "SimpleFontData.h"
#include "FontMetrics.h"
#include "FloatRect.h"
#include "FontDescription.h"
#include <wtf/MathExtras.h>
#include <unicode/uchar.h>
#include <unicode/unorm.h>
#include <mlang.h>

#include "webkit/glue/webkit_glue.h"

namespace WebCore {

static inline float scaleEmToUnits(float x, int unitsPerEm)
{
  return unitsPerEm ? x / (float)unitsPerEm : x;
}

void SimpleFontData::platformInit()
{
    HDC dc = GetDC(0);
    HGDIOBJ oldFont = SelectObject(dc, m_font.hfont());

    m_isSystemFont = false;

    TEXTMETRIC tm = {0};
    if (!GetTextMetrics(dc, &tm)) {
        if (webkit_glue::EnsureFontLoaded(m_font.hfont())) {
            // Retry GetTextMetrics.
            // TODO(nsylvain): Handle gracefully the error if this call also
            // fails.
            // See bug 1136944.
            if (!GetTextMetrics(dc, &tm)) {
                ASSERT_NOT_REACHED();
            }
        }
    }

    m_avgCharWidth = tm.tmAveCharWidth;
    m_maxCharWidth = tm.tmMaxCharWidth;

    if (!m_font.overrideFontMetrics()) {
        m_ascent = tm.tmAscent;
        m_descent = tm.tmDescent;
        m_lineGap = tm.tmExternalLeading;
        m_xHeight = m_ascent * 0.56f;  // Best guess for xHeight for non-Truetype fonts.

        OUTLINETEXTMETRIC otm;
        if (GetOutlineTextMetrics(dc, sizeof(otm), &otm) > 0) {
            // This is a TrueType font.  We might be able to get an accurate xHeight.
            GLYPHMETRICS gm = {0};
            MAT2 mat = {{0, 1}, {0, 0}, {0, 0}, {0, 1}}; // The identity matrix.
            DWORD len = GetGlyphOutlineW(dc, 'x', GGO_METRICS, &gm, 0, 0, &mat);
            if (len != GDI_ERROR && gm.gmBlackBoxY > 0)
                m_xHeight = static_cast<float>(gm.gmBlackBoxY);
        }
    } else {
        // We have cached metrics available from a run of Apple's DumpRenderTree.
        // We use these instead of the system metrics, and scale them the same
        // way that Apple does.

        const FontMetrics* metrics = m_font.overrideFontMetrics();
        int unitsPerEm = metrics->unitsPerEm;
        float pointSize = m_font.size();
        float fAscent = scaleEmToUnits(metrics->ascent, unitsPerEm) * pointSize;
        float fDescent = scaleEmToUnits(metrics->descent, unitsPerEm) * pointSize;
        float fLineGap = scaleEmToUnits(metrics->lineGap, unitsPerEm) * pointSize;
        float fXHeight = scaleEmToUnits(metrics->xHeight, unitsPerEm) * pointSize;

        m_isSystemFont = metrics->isSystemFont;

        // The following comment and code is from FontDataMac.mm (since we're
        // using their metrics, we need to adjust them the same way they do):

        // We need to adjust Times, Helvetica, and Courier to closely match the
        // vertical metrics of their Microsoft counterparts that are the de facto
        // web standard. The AppKit adjustment of 20% is too big and is
        // incorrectly added to line spacing, so we use a 15% adjustment instead
        // and add it to the ascent.
        if (equalIgnoringCase(metrics->family, "Times") ||
            equalIgnoringCase(metrics->family, "Helvetica") ||
            equalIgnoringCase(metrics->family, "Courier")) {
            fAscent += floorf(((fAscent + fDescent) * 0.15f) + 0.5f);
        }

        m_ascent = lroundf(fAscent);
        m_descent = lroundf(fDescent);
        m_lineGap = lroundf(fLineGap);
        m_xHeight = fXHeight;
    }

    m_lineSpacing = m_ascent + m_descent + m_lineGap;

    SelectObject(dc, oldFont);
    ReleaseDC(0, dc);

    // Uniscribe will allocate this for us, and expects it to be initialized to
    // NULL.
    m_scriptCache = 0;

    // It's unclear why this variable is a pointer.
    m_scriptFontProperties = 0;
}

void SimpleFontData::platformDestroy()
{
    if (m_font.isMLangFont()) {
        // We have to release the font instead of just deleting it, since we
        // didn't make it.
        //
        // TODO(brettw): bug 1072298: This should be removed when we remove
        // IMLangFontLink2 support. I think this is incorrect, as it will delete
        // the font out from under any other PlatformData structures that share
        // the same internal refcounted objects.
        IMLangFontLink2* langFontLink = FontCache::getFontLinkInterface();
        if (langFontLink)
            langFontLink->ReleaseFont(m_font.hfont());
    }

    // We don't hash this on Win32, so it's effectively owned by us.
    delete m_smallCapsFontData;
    m_smallCapsFontData = NULL;

    ScriptFreeCache(&m_scriptCache);
    delete m_scriptFontProperties;
}

SimpleFontData* SimpleFontData::smallCapsFontData(const FontDescription& fontDescription) const
{
    if (!m_smallCapsFontData) {
        LOGFONT winfont;
        GetObject(m_font.hfont(), sizeof(LOGFONT), &winfont);
        float smallCapsSize = 0.70f * fontDescription.computedSize();
        // Unlike WebKit trunk, we don't multiply the size by 32.  That seems
        // to be some kind of artifact of their CG backend, or something.
        winfont.lfHeight = -lroundf(smallCapsSize);
        HFONT hfont = CreateFontIndirect(&winfont);
        m_smallCapsFontData =
            new SimpleFontData(FontPlatformData(hfont, smallCapsSize,
                                                m_font.overrideFontMetrics(),
                                                false));
    }
    return m_smallCapsFontData;
}

bool SimpleFontData::containsCharacters(const UChar* characters, int length) const
{
    // FIXME: Microsoft documentation seems to imply that characters can be output using a given font and DC
    // merely by testing code page intersection.  This seems suspect though.  Can't a font only partially
    // cover a given code page?
    IMLangFontLink2* langFontLink = FontCache::getFontLinkInterface();
    if (!langFontLink)
        return false;

    HDC dc = GetDC((HWND)0);
    
    DWORD acpCodePages;
    langFontLink->CodePageToCodePages(CP_ACP, &acpCodePages);

    DWORD fontCodePages;
    langFontLink->GetFontCodePages(dc, m_font.hfont(), &fontCodePages);

    DWORD actualCodePages;
    long numCharactersProcessed;
    long offset = 0;
    while (offset < length) {
        langFontLink->GetStrCodePages(characters, length, acpCodePages, &actualCodePages, &numCharactersProcessed);
        if ((actualCodePages & fontCodePages) == 0)
            return false;
        offset += numCharactersProcessed;
    }

    ReleaseDC(0, dc);

    return true;
}

void SimpleFontData::determinePitch()
{
    // TEXTMETRICS have this.  Set m_treatAsFixedPitch based off that.
    HDC dc = GetDC((HWND)0);
    HGDIOBJ oldFont = SelectObject(dc, m_font.hfont());

    // Yes, this looks backwards, but the fixed pitch bit is actually set if the font
    // is *not* fixed pitch.  Unbelievable but true.
    TEXTMETRIC tm = {0};
    if (!GetTextMetrics(dc, &tm)) {
        if (webkit_glue::EnsureFontLoaded(m_font.hfont())) {
            // Retry GetTextMetrics.
            // TODO(nsylvain): Handle gracefully the error if this call also fails.
            // See bug 1136944.
            if (!GetTextMetrics(dc, &tm)) {
                ASSERT_NOT_REACHED();
            }
        }
    }

    m_treatAsFixedPitch = ((tm.tmPitchAndFamily & TMPF_FIXED_PITCH) == 0);

    SelectObject(dc, oldFont);
    ReleaseDC(0, dc);
}

float SimpleFontData::platformWidthForGlyph(Glyph glyph) const
{
    const FontMetrics* metrics = m_font.overrideFontMetrics();
    if (metrics && glyph) {
        // We have cached metrics available from a run of Apple's DumpRenderTree.
        // We use these during layout tests instead of the system-supplied
        // metrics so that we can match their font size output.

        int width = metrics->getWidthForGlyph(glyph);
        ASSERT(width != -1);
        float pointSize = m_font.size();
        float fWidth = scaleEmToUnits(width, metrics->unitsPerEm) * pointSize;
        float result = fWidth + metrics->syntheticBoldOffset;

        // Don't round for the system font, because Apple doesn't.
        if (!m_isSystemFont)
            result = roundf(result);

#ifdef SUPER_VERBOSE_FONT_DEBUGGING
        fprintf(stderr, "GLYPH %s-%d: %d/%d*%f = %f + %d = %f\n",
                metrics->family.ascii().data(), glyph, width, metrics->unitsPerEm,
                pointSize, fWidth, metrics->syntheticBoldOffset, result);
#endif

        return result;
    }

    HDC dc = GetDC(0);
    HGDIOBJ oldFont = SelectObject(dc, m_font.hfont());

    int width = 0;
    if (!GetCharWidthI(dc, glyph, 1, 0, &width)) {
        // Ask the browser to preload the font and retry.
        if (webkit_glue::EnsureFontLoaded(m_font.hfont())) {
            if (!GetCharWidthI(dc, glyph, 1, 0, &width)) {
                ASSERT_NOT_REACHED();
            }
        }
    }

    SelectObject(dc, oldFont);
    ReleaseDC(0, dc);

    return static_cast<float>(width);
}

SCRIPT_FONTPROPERTIES* SimpleFontData::scriptFontProperties() const
{
    if (!m_scriptFontProperties) {
        m_scriptFontProperties = new SCRIPT_FONTPROPERTIES;
        memset(m_scriptFontProperties, 0, sizeof(SCRIPT_FONTPROPERTIES));
        m_scriptFontProperties->cBytes = sizeof(SCRIPT_FONTPROPERTIES);
        HRESULT result = ScriptGetFontProperties(0, scriptCache(),
                                                 m_scriptFontProperties);
        if (result == E_PENDING) {
            HDC dc = GetDC(0);
            HGDIOBJ oldFont = SelectObject(dc, m_font.hfont());
            HRESULT hr = ScriptGetFontProperties(dc, scriptCache(),
                                                 m_scriptFontProperties);
            if (S_OK != hr) {
                if (webkit_glue::EnsureFontLoaded(m_font.hfont())) {
                    // Retry ScriptGetFontProperties.
                    // TODO(nsylvain): Handle gracefully the error if this call
                    // also fails. See bug 1136944.
                    hr = ScriptGetFontProperties(dc, scriptCache(),
                                                 m_scriptFontProperties);
                    if (S_OK != hr) {
                        ASSERT_NOT_REACHED();
                    }
                }
            }

            SelectObject(dc, oldFont);
            ReleaseDC(0, dc);
        }
    }
    return m_scriptFontProperties;
}

}
