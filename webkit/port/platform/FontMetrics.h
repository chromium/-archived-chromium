// Copyright 2008, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
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
// This file handles reading in the font metric data from the Mac version of
// DumpRenderTree.  We use the sizing info from the Mac so that we can match
// the layout tests.

#ifndef FontMetrics_H__
#define FontMetrics_H__

#include <wtf/Vector.h>
#include <wtf/HashMap.h>
#include "PlatformString.h"

namespace WebCore {

// The following namespace holds definitions shared by
// FontMetrics and FontFallbackMetrics
namespace FontMetricsSharedDefs {

struct RangeAndValue {
    int start, end;
    int value;
};

typedef Vector<RangeAndValue> RangeAndValueList;

// Return V.value such that x >= V.start && x <= V.end [for V in searchSpace].
// Expects searchSpace to be sorted ascending on RangeAndValue.start.
// If no such V exists, returns -1.
int binarySearch(const RangeAndValueList& searchSpace, int x);

} // namespace FontMetricsSharedDefs

class FontMetrics {
public:
    // Returns the font metrics for the given font family and attributes.
    // The pointer returned is owned by the FontMetrics module, and should
    // not be deleted or modified by the caller.
    static const FontMetrics* lookup(const String& family,
                                     bool bold,
                                     bool italic);

    // Returns the glyph for the given character, or 0 if not found.
    int getGlyphForChar(int ch) const;

    // Returns the unscaled width of the given glyph, or -1 if not found.
    int getWidthForGlyph(int glyph) const;

    String family;
    float syntheticBoldOffset;
    bool isSystemFont;

    // The scaling factor of the metrics.
    // metric/unitsPerEm * pointSize = pixel size of the metric.
    int unitsPerEm;

    // The following values should be divided by unitsPerEm to get their
    // size as a fraction of the font point size.
    int ascent;
    int descent;
    int lineGap;
    int xHeight;

private:
    static bool parseFontMetricsFile(FILE* fp, FontMetrics* info);

    // To save space, Glyph widths are grouped into ranges of consecutive
    // glyphs that share the same width.  This mostly matters for the glyphs
    // from 1000-65535 in most fonts.
    typedef FontMetricsSharedDefs::RangeAndValue GlyphRange;

    typedef Vector<GlyphRange> GlyphWidthMap;
    typedef HashMap<int, int> CharToGlyphMap;

    GlyphWidthMap glyphWidths;
    CharToGlyphMap charToGlyphMap;
};

class FontFallbackMetrics {
public:
    // Returns the font family to use as fallback for |family| when drawing
    // character |character|, or NULL.
    // This is used when in layout-test mode to simulate safari-mac's
    // font fallback.
    static const String* lookup(const String& family, UChar character);

private:
    typedef Vector<FontMetricsSharedDefs::RangeAndValue> FallbackRuleList;
    
    static bool parseRulesFile(FILE* fp, FallbackRuleList* fallbackRules);

    // Returns the list of fonts to fallback per character.
    // Or returns NULL if no rules could be be found.
    static FallbackRuleList* getFallbackRules(const String& family);

    // Returns a numeric name for "family"
    // (corresponding to fontIndexToNameMap()[i])
    static int getFontIndex(const String& family);

    typedef Vector<String> FontNameMap;
    static FontNameMap& fontIndexToNameMap();
};

} // namespace WebCore

#endif  // FontMetrics_H__
