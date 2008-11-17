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
#include "UniscribeHelperTextRun.h"

#include "ChromiumBridge.h"
#include "Font.h"
#include "SimpleFontData.h"

namespace WebCore {

UniscribeHelperTextRun::UniscribeHelperTextRun(const WebCore::TextRun& run,
                                               const WebCore::Font& font)
    : UniscribeHelper(run.characters(), run.length(), run.rtl(),
                      font.primaryFont()->platformData().hfont(),
                      font.primaryFont()->platformData().scriptCache(),
                      font.primaryFont()->platformData().scriptFontProperties())
    , m_font(&font)
    , m_fontIndex(0)
{
    setDirectionalOverride(run.directionalOverride());
    setLetterSpacing(font.letterSpacing());
    setSpaceWidth(font.spaceWidth());
    setWordSpacing(font.wordSpacing());
    setAscent(font.primaryFont()->ascent());

    Init();

    // Padding is the amount to add to make justification happen. This
    // should be done after Init() so all the runs are already measured.
    if (run.padding() > 0)
        Justify(run.padding());
}

UniscribeHelperTextRun::UniscribeHelperTextRun(
    const wchar_t* input,
    int inputLength,
    bool isRtl,
    HFONT hfont,
    SCRIPT_CACHE* scriptCache,
    SCRIPT_FONTPROPERTIES* fontProperties)
    : UniscribeHelper(input, inputLength, isRtl, hfont,
                      scriptCache, fontProperties)
    , m_font(NULL)
    , m_fontIndex(-1)
{
}

void UniscribeHelperTextRun::TryToPreloadFont(HFONT font)
{
    // Ask the browser to get the font metrics for this font.
    // That will preload the font and it should now be accessible
    // from the renderer.
    WebCore::ChromiumBridge::ensureFontLoaded(font);
}

bool UniscribeHelperTextRun::NextWinFontData(
    HFONT* hfont,
    SCRIPT_CACHE** scriptCache,
    SCRIPT_FONTPROPERTIES** fontProperties,
    int* ascent)
{
    // This check is necessary because NextWinFontData can be called again
    // after we already ran out of fonts. fontDataAt behaves in a strange
    // manner when the difference between param passed and # of fonts stored in
    // WebKit::Font is larger than one. We can avoid this check by setting
    // font_index_ to # of elements in hfonts_ when we run out of font. In that
    // case, we'd have to go through a couple of more checks before returning
    // false.
    if (m_fontIndex == -1 || !m_font)
        return false;

    // If the font data for a fallback font requested is not yet retrieved, add
    // them to our vectors. Note that '>' rather than '>=' is used to test that
    // condition. primaryFont is not stored in hfonts_, and friends so that
    // indices for fontDataAt and our vectors for font data are 1 off from each
    // other.  That is, when fully populated, hfonts_ and friends have one font
    // fewer than what's contained in font_.
    if (static_cast<size_t>(++m_fontIndex) > m_hfonts.size()) {
        const WebCore::FontData *fontData = m_font->fontDataAt(m_fontIndex);
        if (!fontData) {
            // Ran out of fonts.
            m_fontIndex = -1;
            return false;
        }

        // TODO(ericroman): this won't work for SegmentedFontData
        // http://b/issue?id=1007335
        const WebCore::SimpleFontData* simpleFontData =
            fontData->fontDataForCharacter(' ');

        m_hfonts.append(simpleFontData->platformData().hfont());
        m_scriptCaches.append(
            simpleFontData->platformData().scriptCache());
        m_fontProperties.append(
            simpleFontData->platformData().scriptFontProperties());
        m_ascents.append(simpleFontData->ascent());
    }

    *hfont = m_hfonts[m_fontIndex - 1];
    *scriptCache = m_scriptCaches[m_fontIndex - 1];
    *fontProperties = m_fontProperties[m_fontIndex - 1];
    *ascent = m_ascents[m_fontIndex - 1];
    return true;
}

void UniscribeHelperTextRun::ResetFontIndex()
{
    m_fontIndex = 0;
}

}  // namespace WebCore
