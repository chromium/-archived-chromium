// Copyright (c) 2008, Google Inc.
// All rights reserved.
// 
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
// 
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
// 
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "config.h"
#include "GlyphPageTreeNode.h"

// This PANGO_ENABLE_BACKEND define lets us get at some of the internal Pango
// call which we need. This include must be here otherwise we include pango.h
// via another route (without the define) and that sets the include guard.
// Then, when we try to include it in the future the guard stops us getting the
// functions that we need.
#define PANGO_ENABLE_BACKEND
#include <pango/pango.h>
#include <pango/pangofc-font.h>

#include "Font.h"
#include "NotImplemented.h"
#include "SimpleFontData.h"

namespace WebCore
{

bool GlyphPage::fill(unsigned offset, unsigned length, UChar* buffer, unsigned bufferLength, const SimpleFontData* fontData)
{
    // The bufferLength will be greater than the glyph page size if the buffer has Unicode supplementary characters.
    // We won't support this for now.
    if (bufferLength > GlyphPage::size)
        return false;

    if (!fontData->m_font.m_font || fontData->m_font.m_font == reinterpret_cast<PangoFont*>(-1))
        return false;

    bool haveGlyphs = false;
    for (unsigned i = 0; i < length; i++) {
        Glyph glyph = pango_fc_font_get_glyph(PANGO_FC_FONT(fontData->m_font.m_font), buffer[i]);
        if (!glyph)
            setGlyphDataForIndex(offset + i, 0, 0);
        else {
            setGlyphDataForIndex(offset + i, glyph, fontData);
            haveGlyphs = true;
        }
    }

    return haveGlyphs;
}

}  // namespace WebCore
