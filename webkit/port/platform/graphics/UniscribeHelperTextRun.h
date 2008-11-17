// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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

#ifndef UniscribeHelperTextRun_h
#define UniscribeHelperTextRun_h

#include "UniscribeHelper.h"

namespace WebCore {

class Font;
class TextRun;

// Wrapper around the Uniscribe helper that automatically sets it up with the
// WebKit types we supply.
class UniscribeHelperTextRun : public UniscribeHelper {
public:
    // Regular constructor used for WebCore text run processing.
    UniscribeHelperTextRun(const WebCore::TextRun& run,
                           const WebCore::Font& font);

    // Constructor with the same interface as the gfx::UniscribeState. Using
    // this constructor will not give you font fallback, but it will provide
    // the ability to load fonts that may not be in the OS cache
    // ("TryToPreloadFont") if the caller does not have a TextRun/Font.
    UniscribeHelperTextRun(const wchar_t* input,
                           int inputLength,
                           bool isRtl,
                           HFONT hfont,
                           SCRIPT_CACHE* scriptCache,
                           SCRIPT_FONTPROPERTIES* fontProperties);

protected:
    virtual void TryToPreloadFont(HFONT font);

private:
    // This function retrieves the Windows font data (HFONT, etc) for the next
    // WebKit font in the list. If the font data corresponding to font_index_
    // has been obtained before, returns the values stored in our internal
    // vectors (hfonts_, etc).  Otherwise, it gets next SimpleFontData from
    // WebKit and adds them to in hfonts_ and friends so that font data can be
    // returned quickly next time they're requested.
    virtual bool NextWinFontData(HFONT* hfont,
                                 SCRIPT_CACHE** scriptCache,
                                 SCRIPT_FONTPROPERTIES** fontProperties,
                                 int* ascent);
    virtual void ResetFontIndex();

    // Reference to WebKit::Font that contains all the information about fonts
    // we can use to render this input run of text.  It is used in
    // NextWinFontData to retrieve Windows font data for a series of
    // non-primary fonts.
    //
    // This pointer can be NULL for no font fallback handling.
    const Font* m_font;

    // It's rare that many fonts are listed in stylesheets.
    // Four would be large enough in most cases.
    const static size_t kNumberOfFonts = 4;

    // These vectors are used to store Windows font data for non-primary fonts.
    Vector<HFONT, kNumberOfFonts> m_hfonts;
    Vector<SCRIPT_CACHE*, kNumberOfFonts> m_scriptCaches;
    Vector<SCRIPT_FONTPROPERTIES*, kNumberOfFonts> m_fontProperties;
    Vector<int, kNumberOfFonts> m_ascents;

    // Index of the fallback font we're currently using for NextWinFontData.
    int m_fontIndex;
};

}  // namespace WebCore

#endif  // UniscribeHelperTextRun_h
