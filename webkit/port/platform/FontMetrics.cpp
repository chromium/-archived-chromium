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
#include "FontMetrics.h"

#include <map>
#include <string>

#include "base/file_util.h"
#include "base/string_util.h"
#include "webkit/glue/glue_util.h"
#include "webkit/glue/webkit_glue.h"

// Notes about the .afm file format:
// - Very unforgiving.
// - Metrics given on the right are scaled by the 'unitsPerEm' value: if
//   unitsPerEm = 2048, then a width of 1024 corresponds to .5.  This is
//   then multiplied by the point size to find the final metric.
// - The glyph width section must be sorted by starting glyph.
// - The glyph width section must cover the full range of glyphs from [1,65535].

namespace WebCore {

// static
bool FontMetrics::parseFontMetricsFile(FILE* fp, FontMetrics* info)
{
    if (fscanf_s(fp, "unitsPerEm = %d\n", &info->unitsPerEm) != 1)
        return false;
    if (fscanf_s(fp, "ascent = %d\n", &info->ascent) != 1)
        return false;
    if (fscanf_s(fp, "descent = %d\n", &info->descent) != 1)
        return false;
    if (fscanf_s(fp, "lineGap = %d\n", &info->lineGap) != 1)
        return false;
    if (fscanf_s(fp, "xHeight = %d\n", &info->xHeight) != 1)
        return false;

    int numGlyphs;
    if (fscanf_s(fp, "numGlyphs = %d\n", &numGlyphs) != 1)
        return false;

    for (int i = 0; i < numGlyphs; ++i) {
        FontMetrics::GlyphRange range;
        if (fscanf_s(fp, "[%d,%d] = %d\n",
                     &range.start, &range.end, &range.value) != 3) {
            return false;
        }
        info->glyphWidths.append(range);
    }

    int numChars;
    if (fscanf_s(fp, "numChars = %d\n", &numChars) != 1)
        return false;

    for (int i = 0; i < numChars; ++i) {
        int ch, glyph;
        if (fscanf_s(fp, "%d = %d\n", &ch, &glyph) != 2)
            return false;
        info->charToGlyphMap.set(ch, glyph);
    }

    return true;
}

// static
const FontMetrics* FontMetrics::lookup(const String& family,
                                       bool bold,
                                       bool italic)
{
    // FontMetrics exist only during layout tests.
    if (!webkit_glue::IsLayoutTestMode())
        return NULL;

    typedef std::map<std::wstring, FontMetrics*> FontMetricsMap;
    static FontMetricsMap fontMetricsCache;

    // Generate the filename: "<family>[b][i].afm"
    String lower_family = family.lower();
    std::wstring filename = webkit_glue::StringToStdWString(lower_family);
    std::replace(filename.begin(), filename.end(), L' ', L'_');
    if (bold)
        filename += 'b';
    if (italic)
        filename += 'i';
    filename += L".afm";

    if (fontMetricsCache.find(filename) != fontMetricsCache.end())
        return fontMetricsCache[filename];

    std::wstring path;
    webkit_glue::GetApplicationDirectory(&path);
    file_util::AppendToPath(&path, L"fonts");
    file_util::AppendToPath(&path, filename);

    FILE* fp;
    fopen_s(&fp, WideToUTF8(path).c_str(), "r");
    if (!fp) {
        // Try to fall back on a font without bold/italic.
        // Note: the order of these fallback cases is important.  A bold+italic
        // font should fall back to bold (with synthetic italics, which doesn't
        // appear to influence layout) if it doesn't exist.  This seems to match
        // what Apple does on Windows.
        if (italic)
            return lookup(family, bold, false);
        if (bold) {
            // If we want bold but don't have it, do as Apple would do and use a
            // synethetic offset.
            const FontMetrics* info = lookup(family, false, italic);
            if (info) {
                FontMetrics* bold_info = new FontMetrics(*info);
                bold_info->syntheticBoldOffset = 1.0f;
                fontMetricsCache[filename] = bold_info;
                return bold_info;
            }
            return NULL;
        }
        return NULL;
    }

    // Read in the data.
    FontMetrics* info = new FontMetrics;
    info->family = lower_family;
    info->syntheticBoldOffset = 0.0f;
    info->isSystemFont = (lower_family == "lucida grande");

    if (!parseFontMetricsFile(fp, info)) {
        LOG_ERROR("FontMetrics::lookup: bad file format for file '%S'",
                  path.c_str());
        ASSERT_NOT_REACHED();

        delete info;
        info = NULL;
    }

    fclose(fp);
    fontMetricsCache[filename] = info;
    return info;
}

int FontMetrics::getGlyphForChar(int ch) const
{
    return charToGlyphMap.get(ch);
}

int FontMetricsSharedDefs::binarySearch(const RangeAndValueList& searchSpace, int x)
{
    // Do a binary search on the start of each range, grab the one that |x|
    // falls inside.
    // NOTE: This assumes searchSpace is sorted by |start|.

    int low = 0;
    int high = static_cast<int>(searchSpace.size()-1);
    while (low <= high) {
        int mid = low + (high - low) / 2;
        const RangeAndValue& range = searchSpace[mid];
        if (range.start <= x && range.end >= x)
            return range.value;

        if (x < range.start)
            high = mid - 1;
        else
            low = mid + 1;
    }

    return -1;
}

// static
int FontMetrics::getWidthForGlyph(int glyph) const
{
  return FontMetricsSharedDefs::binarySearch(glyphWidths, glyph);
}

// static
const String* FontFallbackMetrics::lookup(const String& family, UChar character)
{
    // FontFallbackMetrics exist only during layout tests.
    if (!webkit_glue::IsLayoutTestMode())
        return NULL;

    const FallbackRuleList* fallbackRules = getFallbackRules(family);
    if (!fallbackRules)
        return NULL;

    int fontIndex = binarySearch(*fallbackRules, character);
    if (fontIndex == -1)
        return NULL;

    return &fontIndexToNameMap()[fontIndex];
}

// static
FontFallbackMetrics::FallbackRuleList* FontFallbackMetrics::getFallbackRules(const String& family)
{
    // Build the file name for the font's fallback rules.
    std::wstring filename = webkit_glue::StringToStdWString(family.lower());
    std::replace(filename.begin(), filename.end(), L' ', L'_');
    filename += L"-fallback.txt";

    // If we have already parsed the fallback rules file, return the cached version.
    typedef std::map<std::wstring, FallbackRuleList*> FontToFallbackRulesMap;
    static FontToFallbackRulesMap fallbackRulesCache;
    if (fallbackRulesCache.find(filename) != fallbackRulesCache.end())
        return fallbackRulesCache[filename];

    // Open the fallback rules file for reading.
    std::wstring path;
    webkit_glue::GetApplicationDirectory(&path);
    file_util::AppendToPath(&path, L"fonts");
    file_util::AppendToPath(&path, filename);
    
    FILE* fp;
    fopen_s(&fp, WideToUTF8(path).c_str(), "r");

    FallbackRuleList* rulesList = NULL;

    if (!fp) {
        LOG_ERROR("FontFallbackMetrics::lookup: no fallback rules file: '%S'",
            path.c_str());
    } else {
        // Parse and close the fallback rules file.
        rulesList = new FallbackRuleList;
        bool successfullyParsed = parseRulesFile(fp, rulesList);
        fclose(fp);

        if (!successfullyParsed) {
            delete rulesList;
            rulesList = NULL;
            LOG_ERROR("FontFallbackMetrics::lookup: bad file format: '%S'",
                      path.c_str());
            ASSERT_NOT_REACHED();
        }
    }

    fallbackRulesCache[filename] = rulesList;
    return rulesList;
}

// static
bool FontFallbackMetrics::parseRulesFile(FILE* fp, FontFallbackMetrics::FallbackRuleList* fallbackRules)
{
    int numFonts;
    if (fscanf_s(fp, "numFonts = %d\n", &numFonts) != 1)
        return false;

    // Extract all the fonts from the file, and map them to an entry in
    // fontIndexToNameMap(). (Saves space by not duplicating font names)
    std::map<int, int> localFontIndexToGlobalIndexMap;
    for (int i = 0; i < numFonts; ++i) {
        int fontIndex;
        if (fscanf_s(fp, "%d = ", &fontIndex) != 1)
            return false;
        // use fgets instead of fscanf_s to read the font name,
        // since it may contain spaces.
        const unsigned kFontNameBufSize = 80;
        char fontNameBuf[kFontNameBufSize];
        if (!fgets(fontNameBuf, kFontNameBufSize, fp))
            return false;

         // Strip the newline
        String fontName(fontNameBuf, strlen(fontNameBuf) - 1);
        localFontIndexToGlobalIndexMap[fontIndex] = getFontIndex(fontName);
    }

    int numRules;
    if (fscanf_s(fp, "numRules = %d\n", &numRules) != 1)
        return false;

    for (int i = 0; i < numRules; ++i) {
        FontMetricsSharedDefs::RangeAndValue rule;
        int localFontIndex;
        if (fscanf_s(fp, "[%d,%d] = %d\n",
                     &rule.start, &rule.end, &localFontIndex) != 3) {
            return false;
        }
        // Set the font index that it references.
        rule.value = localFontIndexToGlobalIndexMap[localFontIndex];
        fallbackRules->append(rule);
    }

    return true;
}

// static
FontFallbackMetrics::FontNameMap& FontFallbackMetrics::fontIndexToNameMap()
{
    static FontNameMap map;
    return map;
}

// static
int FontFallbackMetrics::getFontIndex(const String& family)
{
    FontNameMap& indexToFontMap = fontIndexToNameMap();

    for (size_t i = 0; i < indexToFontMap.size(); ++i) {
        if (indexToFontMap[i] == family)
            return i;
    }
    indexToFontMap.append(family);
    return indexToFontMap.size() - 1;
}

} // namespace WebCore
