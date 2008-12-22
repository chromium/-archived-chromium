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
#include "FontCache.h"

#include "ChromiumBridge.h"
#include "Font.h"
#include "FontUtilsChromiumWin.h"
#include "HashMap.h"
#include "HashSet.h"
#include "SimpleFontData.h"
#include "StringHash.h"
#include <unicode/uniset.h>

#include <windows.h>
#include <objidl.h>
#include <mlang.h>

using std::min;

namespace WebCore
{

void FontCache::platformInit()
{
    // Not needed on Windows.
}

// FIXME(jungshik) : consider adding to WebKit String class
static bool isStringASCII(const String& s)
{
    for (int i = 0; i < static_cast<int>(s.length()); ++i) {
        if (s[i] > 0x7f)
            return false;
    }
    return true;
}

// When asked for a CJK font with a native name under a non-CJK locale or
// asked for a CJK font with a Romanized name under a CJK locale,
// |GetTextFace| (after |CreateFont*|) returns a 'bogus' value (e.g. Arial).
// This is not consistent with what MSDN says !!
// Therefore, before we call |CreateFont*|, we have to map a Romanized name to 
// the corresponding native name under a CJK locale and vice versa 
// under a non-CJK locale.
// See the corresponding gecko bugs at 
// https://bugzilla.mozilla.org/show_bug.cgi?id=373952
// https://bugzilla.mozilla.org/show_bug.cgi?id=231426
static bool LookupAltName(const String& name, String& altName)
{
    struct FontCodepage {
        WCHAR *name;
        int   codePage;
    };

    struct NamePair {
        WCHAR *name;
        FontCodepage altNameCp; 
    };

    // FIXME(jungshik) : This list probably covers 99% of cases. 
    // To cover the remaining 1% and cut down the file size, 
    // consider accessing 'NAME' table of a truetype font
    // using |GetFontData| and caching the mapping. 
    // 932 : Japanese, 936 : Simp. Chinese, 949 : Korean, 950 : Trad. Chinese
    // In the table below, the ASCII keys are all lower-cased for
    // case-insensitive matching.
    static const NamePair namePairs[] = {
        // ＭＳ Ｐゴシック, MS PGothic
        {L"\xFF2D\xFF33 \xFF30\x30B4\x30B7\x30C3\x30AF", {L"MS PGothic", 932}},
        {L"ms pgothic", {L"\xFF2D\xFF33 \xFF30\x30B4\x30B7\x30C3\x30AF", 932}},
        // ＭＳ Ｐ明朝, MS PMincho
        {L"\xFF2D\xFF33 \xFF30\x660E\x671D", {L"MS PMincho", 932}},
        {L"ms pmincho", {L"\xFF2D\xFF33 \xFF30\x660E\x671D", 932}},
        // ＭＳゴシック, MS Gothic
        {L"\xFF2D\xFF33 \x30B4\x30B7\x30C3\x30AF", {L"MS Gothic", 932}},
        {L"ms gothic", {L"\xFF2D\xFF33 \x30B4\x30B7\x30C3\x30AF", 932}},
        // ＭＳ 明朝, MS Mincho
        {L"\xFF2D\xFF33 \x660E\x671D", {L"MS Mincho", 932}},
        {L"ms mincho", {L"\xFF2D\xFF33 \x660E\x671D", 932}},
        // メイリオ, Meiryo
        {L"\x30E1\x30A4\x30EA\x30AA", {L"Meiryo", 932}},
        {L"meiryo", {L"\x30E1\x30A4\x30EA\x30AA", 932}},
        // 바탕, Batang
        {L"\xBC14\xD0D5", {L"Batang", 949}},
        {L"batang", {L"\xBC14\xD0D5", 949}},
        // 바탕체, Batangche
        {L"\xBC14\xD0D5\xCCB4", {L"Batangche", 949}},
        {L"batangche", {L"\xBC14\xD0D5\xCCB4", 949}},
        // 굴림, Gulim
        {L"\xAD74\xB9BC", {L"Gulim", 949}},
        {L"gulim", {L"\xAD74\xB9BC", 949}},
        // 굴림체, Gulimche
        {L"\xAD74\xB9BC\xCCB4", {L"Gulimche", 949}},
        {L"gulimche", {L"\xAD74\xB9BC\xCCB4", 949}},
        // 돋움, Dotum
        {L"\xB3CB\xC6C0", {L"Dotum", 949}},
        {L"dotum", {L"\xB3CB\xC6C0", 949}},
        // 돋움체, Dotumche
        {L"\xB3CB\xC6C0\xCCB4", {L"Dotumche", 949}},
        {L"dotumche", {L"\xB3CB\xC6C0\xCCB4", 949}},
        // 궁서, Gungsuh
        {L"\xAD81\xC11C", {L"Gungsuh", 949}},
        {L"gungsuh", {L"\xAD81\xC11C", 949}},
        // 궁서체, Gungsuhche
        {L"\xAD81\xC11C\xCCB4", {L"Gungsuhche", 949}},
        {L"gungsuhche", {L"\xAD81\xC11C\xCCB4", 949}},
        // 맑은 고딕, Malgun Gothic
        {L"\xB9D1\xC740 \xACE0\xB515", {L"Malgun Gothic", 949}},
        {L"malgun gothic", {L"\xB9D1\xC740 \xACE0\xB515", 949}},
        // 宋体, SimSun
        {L"\x5B8B\x4F53", {L"SimSun", 936}},
        {L"simsun", {L"\x5B8B\x4F53", 936}},
        // 黑体, SimHei
        {L"\x9ED1\x4F53", {L"SimHei", 936}},
        {L"simhei", {L"\x9ED1\x4F53", 936}},
        // 新宋体, NSimSun
        {L"\x65B0\x5B8B\x4F53", {L"NSimSun", 936}},
        {L"nsimsun", {L"\x65B0\x5B8B\x4F53", 936}},
        // 微软雅黑, Microsoft Yahei
        {L"\x5FAE\x8F6F\x96C5\x9ED1", {L"Microsoft Yahei", 936}},
        {L"microsoft yahei", {L"\x5FAE\x8F6F\x96C5\x9ED1", 936}},
        // 仿宋, FangSong
        {L"\x4EFF\x5B8B",  {L"FangSong", 936}},
        {L"fangsong", {L"\x4EFF\x5B8B", 936}},
        // 楷体, KaiTi
        {L"\x6977\x4F53", {L"KaiTi", 936}},
        {L"kaiti", {L"\x6977\x4F53", 936}},
        // 仿宋_GB2312, FangSong_GB2312
        {L"\x4EFF\x5B8B_GB2312",  {L"FangSong_GB2312", 936}},
        {L"fangsong_gb2312", {L"\x4EFF\x5B8B_gb2312", 936}},
        // 楷体_GB2312, KaiTi_GB2312
        {L"\x6977\x4F53", {L"KaiTi_GB2312", 936}},
        {L"kaiti_gb2312", {L"\x6977\x4F53_gb2312", 936}},
        // 新細明體, PMingLiu
        {L"\x65B0\x7D30\x660E\x9AD4", {L"PMingLiu", 950}},
        {L"pmingliu", {L"\x65B0\x7D30\x660E\x9AD4", 950}},
        // 細明體, MingLiu
        {L"\x7D30\x660E\x9AD4", {L"MingLiu", 950}},
        {L"mingliu", {L"\x7D30\x660E\x9AD4", 950}},
        // 微軟正黑體, Microsoft JhengHei
        {L"\x5FAE\x8EDF\x6B63\x9ED1\x9AD4", {L"Microsoft JhengHei", 950}},
        {L"microsoft jhengHei", {L"\x5FAE\x8EDF\x6B63\x9ED1\x9AD4", 950}},
        // 標楷體, DFKai-SB
        {L"\x6A19\x6977\x9AD4", {L"DFKai-SB", 950}},
        {L"dfkai-sb", {L"\x6A19\x6977\x9AD4", 950}},
        // WenQuanYi Zen Hei
        {L"\x6587\x6cc9\x9a5b\x6b63\x9ed1", {L"WenQuanYi Zen Hei", 950}},
        {L"wenquanyi zen hei", {L"\x6587\x6cc9\x9a5b\x6b63\x9ed1", 950}},
        // WenQuanYi Zen Hei
        {L"\x6587\x6cc9\x9a7f\x6b63\x9ed1", {L"WenQuanYi Zen Hei", 936}},
        {L"wenquanyi zen hei", {L"\x6587\x6cc9\x9a7f\x6b63\x9ed1", 936}},
        // AR PL ShanHeiSun Uni,
        {L"\x6587\x9f0e\x0050\x004c\x7d30\x4e0a\x6d77\x5b8b\x0055\x006e\x0069",
         {L"AR PL ShanHeiSun Uni", 950}},
        {L"ar pl shanheisun uni",
         {L"\x6587\x9f0e\x0050\x004c\x7d30\x4e0a\x6d77\x5b8b\x0055\x006e\x0069", 950}},
        // AR PL ShanHeiSun Uni,
        {L"\x6587\x9f0e\x0050\x004c\x7ec6\x4e0a\x6d77\x5b8b\x0055\x006e\x0069",
         {L"AR PL ShanHeiSun Uni", 936}},
        {L"ar pl shanheisun uni",
         {L"\x6587\x9f0e\x0050\x004c\x7ec6\x4e0a\x6d77\x5b8b\x0055\x006e\x0069", 936}},
        // AR PL ZenKai Uni
        // Traditional Chinese (950) and Simplified Chinese (936) names are
        // identical.
        {L"\x6587\x0050\x004C\x4E2D\x6977\x0055\x006E\x0069", {L"AR PL ZenKai Uni", 950}},
        {L"ar pl zenkai uni", {L"\x6587\x0050\x004C\x4E2D\x6977\x0055\x006E\x0069", 950}},
        {L"\x6587\x0050\x004C\x4E2D\x6977\x0055\x006E\x0069", {L"AR PL ZenKai Uni", 936}},
        {L"ar pl zenkai uni", {L"\x6587\x0050\x004C\x4E2D\x6977\x0055\x006E\x0069", 936}},
    };

    typedef HashMap<String, const FontCodepage*> NameMap;
    static NameMap* fontNameMap = NULL;

    if (!fontNameMap) {
        size_t numElements = sizeof(namePairs) / sizeof(NamePair);
        fontNameMap = new NameMap;
        for (size_t i = 0; i < numElements; ++i)
            fontNameMap->set(String(namePairs[i].name), &(namePairs[i].altNameCp));
    }

    bool isAscii = false; 
    String n;
    // use |lower| only for ASCII names 
    // For non-ASCII names, we don't want to invoke an expensive 
    // and unnecessary |lower|. 
    if (isStringASCII(name)) {
        isAscii = true;
        n = name.lower();
    } else
        n = name;

    NameMap::iterator iter = fontNameMap->find(n);
    if (iter == fontNameMap->end())
        return false;

    static int systemCp = ::GetACP();
    int fontCp = iter->second->codePage;

    if ((isAscii && systemCp == fontCp) || (!isAscii && systemCp != fontCp)) {
        altName = String(iter->second->name);
        return true;
    }

    return false;
}

static HFONT createFontIndirectAndGetWinName(const String& family,
                                             LOGFONT* winfont, String* winName)
{
    int len = min(static_cast<int>(family.length()), LF_FACESIZE - 1);
    memcpy(winfont->lfFaceName, family.characters(), len * sizeof(WORD));
    winfont->lfFaceName[len] = '\0';

    HFONT hfont = CreateFontIndirect(winfont);
    if (!hfont)
        return NULL;

    HDC dc = GetDC(0);
    HGDIOBJ oldFont = static_cast<HFONT>(SelectObject(dc, hfont));
    WCHAR name[LF_FACESIZE];
    unsigned resultLength = GetTextFace(dc, LF_FACESIZE, name);
    if (resultLength > 0)
        resultLength--; // ignore the null terminator

    SelectObject(dc, oldFont);
    ReleaseDC(0, dc);
    *winName = String(name, resultLength);
    return hfont;
}

// This maps font family names to their repertoires of supported Unicode
// characters. Because it's family names rather than font faces we use
// as keys, there might be edge cases where one face of a font family
// has a different repertoire from another face of the same family. 
typedef HashMap<const wchar_t*, UnicodeSet*> FontCmapCache;

static bool fontContainsCharacter(const FontPlatformData* font_data,
                                  const wchar_t* family, UChar32 character)
{
    // TODO(jungshik) : For non-BMP characters, GetFontUnicodeRanges is of
    // no use. We have to read directly from the cmap table of a font.
    // Return true for now.
    if (character > 0xFFFF)
        return true;

    // This cache is just leaked on shutdown.
    static FontCmapCache* fontCmapCache = NULL;
    if (!fontCmapCache)
        fontCmapCache = new FontCmapCache;

    HashMap<const wchar_t*, UnicodeSet*>::iterator it =
        fontCmapCache->find(family);
    if (it != fontCmapCache->end()) 
        return it->second->contains(character);
    
    HFONT hfont = font_data->hfont(); 
    HDC hdc = GetDC(0);
    HGDIOBJ oldFont = static_cast<HFONT>(SelectObject(hdc, hfont));
    int count = GetFontUnicodeRanges(hdc, 0);
    if (count == 0 && ChromiumBridge::ensureFontLoaded(hfont))
        count = GetFontUnicodeRanges(hdc, 0);
    if (count == 0) {
        ASSERT_NOT_REACHED();
        SelectObject(hdc, oldFont);
        ReleaseDC(0, hdc);
        return true;
    }

    static Vector<char, 512> glyphsetBuffer;
    glyphsetBuffer.resize(GetFontUnicodeRanges(hdc, 0));
    GLYPHSET* glyphset = reinterpret_cast<GLYPHSET*>(glyphsetBuffer.data());
    // In addition, refering to the OS/2 table and converting the codepage list
    // to the coverage map might be faster. 
    count = GetFontUnicodeRanges(hdc, glyphset);
    ASSERT(count > 0);
    SelectObject(hdc, oldFont);
    ReleaseDC(0, hdc);

    // TODO(jungshik) : consider doing either of the following two:
    // 1) port back ICU 4.0's faster look-up code for UnicodeSet
    // 2) port Mozilla's CompressedCharMap or gfxSparseBitset
    unsigned i = 0;
    UnicodeSet* cmap = new UnicodeSet;
    while (i < glyphset->cRanges) {
        WCHAR start = glyphset->ranges[i].wcLow; 
        cmap->add(start, start + glyphset->ranges[i].cGlyphs - 1);
        i++;
    }
    cmap->freeze();
    // We don't lowercase |family| because all of them are under our control
    // and they're already lowercased. 
    fontCmapCache->set(family, cmap); 
    return cmap->contains(character);
}

// Given the desired base font, this will create a SimpleFontData for a specific
// font that can be used to render the given range of characters.
const SimpleFontData* FontCache::getFontDataForCharacters(const Font& font,
                                                    const UChar* characters,
                                                    int length)
{
    // TODO(jungshik) : Consider passing fontDescription.dominantScript()
    // to GetFallbackFamily here.
    FontDescription fontDescription = font.fontDescription();
    UChar32 c;
    UScriptCode script;
    const wchar_t* family = getFallbackFamily(characters, length,
        fontDescription.genericFamily(), &c, &script);
    FontPlatformData* data = NULL;
    if (family) {
        data = getCachedFontPlatformData(font.fontDescription(), 
                                         AtomicString(family, wcslen(family)),
                                         false); 
    }

    // Last resort font list : PanUnicode. CJK fonts have a pretty
    // large repertoire. Eventually, we need to scan all the fonts
    // on the system to have a Firefox-like coverage.
    // Make sure that all of them are lowercased.
    const static wchar_t* const cjkFonts[] = {
        L"arial unicode ms",
        L"ms pgothic",
        L"simsun",
        L"gulim",
        L"pmingliu",
        L"wenquanyi zen hei", // partial CJK Ext. A coverage but more
                              // widely known to Chinese users.
        L"ar pl shanheisun uni",
        L"ar pl zenkai uni",
        L"han nom a",  // Complete CJK Ext. A coverage
        L"code2000",   // Complete CJK Ext. A coverage
        // CJK Ext. B fonts are not listed here because it's of no use
        // with our current non-BMP character handling because we use
        // Uniscribe for it and that code path does not go through here.
    };

    const static wchar_t* const commonFonts[] = {
        L"tahoma",
        L"arial unicode ms",
        L"lucida sans unicode",
        L"microsoft sans serif",
        L"palatino linotype",
        // Four fonts below (and code2000 at the end) are not from MS, but
        // once installed, cover a very wide range of characters.
        L"freeserif",
        L"freesans",
        L"gentium",
        L"gentiumalt",
        L"ms pgothic",
        L"simsun",
        L"gulim",
        L"pmingliu",
        L"code2000",
    };

    const wchar_t* const* panUniFonts = NULL;
    int numFonts = 0;
    if (script == USCRIPT_HAN) {
        panUniFonts = cjkFonts;
        numFonts = ARRAYSIZE(cjkFonts);
    } else {
        panUniFonts = commonFonts;
        numFonts = ARRAYSIZE(commonFonts);
    }
    // Font returned from GetFallbackFamily may not cover |characters|
    // because it's based on script to font mapping. This problem is
    // critical enough for non-Latin scripts (especially Han) to
    // warrant an additional (real coverage) check with fontCotainsCharacter.
    int i;
    for (i = 0; (!data || !fontContainsCharacter(data, family, c))
         && i < numFonts; ++i) {
        family = panUniFonts[i]; 
        data = getCachedFontPlatformData(font.fontDescription(),
                                         AtomicString(family, wcslen(family)));
    }
    if (i < numFonts) // we found the font that covers this character !
       return getCachedFontData(data);

    return NULL;

}

const AtomicString& FontCache::alternateFamilyName(const AtomicString& familyName)
{
    // Note that mapping to Courier is removed because 
    // because it's a bitmap font on Windows.
    // Alias Courier -> Courier New
    static AtomicString courier("Courier"), courierNew("Courier New");
    if (equalIgnoringCase(familyName, courier))
        return courierNew;

    // Alias Times <-> Times New Roman.
    static AtomicString times("Times"), timesNewRoman("Times New Roman");
    if (equalIgnoringCase(familyName, times))
        return timesNewRoman;
    if (equalIgnoringCase(familyName, timesNewRoman))
        return times;
    
    // Alias Helvetica <-> Arial
    static AtomicString arial("Arial"), helvetica("Helvetica");
    if (equalIgnoringCase(familyName, helvetica))
        return arial;
    if (equalIgnoringCase(familyName, arial))
        return helvetica;

    // We block bitmap fonts altogether so that we have to 
    // alias MS Sans Serif (bitmap font) -> Microsoft Sans Serif (truetype font)
    static AtomicString msSans("MS Sans Serif");
    static AtomicString microsoftSans("Microsoft Sans Serif");
    if (equalIgnoringCase(familyName, msSans))
        return microsoftSans;

    // Alias MS Serif (bitmap) -> Times New Roman (truetype font). There's no 
    // 'Microsoft Sans Serif-equivalent' for Serif. 
    static AtomicString msSerif("MS Serif");
    if (equalIgnoringCase(familyName, msSerif))
        return timesNewRoman;

    // TODO(jungshik) : should we map 'system' to something ('Tahoma') ? 
    return emptyAtom;
}

FontPlatformData* FontCache::getSimilarFontPlatformData(const Font& font)
{
    return 0;
}

FontPlatformData* FontCache::getLastResortFallbackFont(
    const FontDescription& description)
{
    FontDescription::GenericFamilyType generic = description.genericFamily();
    // TODO(jungshik): Mapping webkit generic to GenericFamilyType needs to
    // be more intelligent. 
    // This spot rarely gets reached. GetFontDataForCharacters() gets hit a lot
    // more often (see TODO comment there). 
    const wchar_t* family = getFontFamilyForScript(description.dominantScript(),
        generic);

    if (family)
        return getCachedFontPlatformData(description, AtomicString(family, wcslen(family)));

    // FIXME: Would be even better to somehow get the user's default font here.
    // For now we'll pick the default that the user would get without changing
    // any prefs.
    static AtomicString timesStr("Times New Roman");
    static AtomicString courierStr("Courier New");
    static AtomicString arialStr("Arial");

    AtomicString& fontStr = timesStr;
    if (generic == FontDescription::SansSerifFamily)
        fontStr = arialStr;
    else if (generic == FontDescription::MonospaceFamily)
        fontStr = courierStr;

    return getCachedFontPlatformData(description, fontStr);
}

static LONG toGDIFontWeight(FontWeight fontWeight)
{
    static LONG gdiFontWeights[] = {
        FW_THIN,        // FontWeight100
        FW_EXTRALIGHT,  // FontWeight200
        FW_LIGHT,       // FontWeight300
        FW_NORMAL,      // FontWeight400
        FW_MEDIUM,      // FontWeight500
        FW_SEMIBOLD,    // FontWeight600
        FW_BOLD,        // FontWeight700
        FW_EXTRABOLD,   // FontWeight800
        FW_HEAVY        // FontWeight900
    };
    return gdiFontWeights[fontWeight];
}

// TODO(jungshik): This may not be the best place to put this function. See
// TODO in pending/FontCache.h.
AtomicString FontCache::getGenericFontForScript(UScriptCode script, const FontDescription& description)
{
    const wchar_t* scriptFont = getFontFamilyForScript(
        script, description.genericFamily());
    return scriptFont ? AtomicString(scriptFont, wcslen(scriptFont)) : emptyAtom;
}

static void FillLogFont(const FontDescription& fontDescription, LOGFONT* winfont)
{
    // The size here looks unusual.  The negative number is intentional.
    // Unlike WebKit trunk, we don't multiply the size by 32.  That seems to be
    // some kind of artifact of their CG backend, or something.
    winfont->lfHeight = -fontDescription.computedPixelSize();
    winfont->lfWidth = 0;
    winfont->lfEscapement = 0;
    winfont->lfOrientation = 0;
    winfont->lfUnderline = false;
    winfont->lfStrikeOut = false;
    winfont->lfCharSet = DEFAULT_CHARSET;
    winfont->lfOutPrecision = OUT_TT_ONLY_PRECIS;
    winfont->lfQuality = ChromiumBridge::layoutTestMode() ? NONANTIALIASED_QUALITY
        : DEFAULT_QUALITY; // Honor user's desktop settings.
    winfont->lfPitchAndFamily = DEFAULT_PITCH | FF_DONTCARE;
    winfont->lfItalic = fontDescription.italic();
    winfont->lfWeight = toGDIFontWeight(fontDescription.weight());
}

bool FontCache::fontExists(const FontDescription& fontDescription, const AtomicString& family)
{
    LOGFONT winfont = {0};
    FillLogFont(fontDescription, &winfont);
    String winName;
    HFONT hfont = createFontIndirectAndGetWinName(family, &winfont, &winName);
    if (!hfont)
      return false;

    DeleteObject(hfont);
    if (equalIgnoringCase(family, winName))
      return true;
    // For CJK fonts with both English and native names, 
    // GetTextFace returns a native name under the font's "locale"
    // and an English name under other locales regardless of 
    // lfFaceName field of LOGFONT. As a result, we need to check
    // if a font has an alternate name. If there is, we need to
    // compare it with what's requested in the first place.
    String altName;
    return LookupAltName(family, altName) && equalIgnoringCase(altName, winName);
}

struct TraitsInFamilyProcData {
    TraitsInFamilyProcData(const AtomicString& familyName)
        : m_familyName(familyName)
    {
    }

    const AtomicString& m_familyName;
    HashSet<unsigned> m_traitsMasks;
};

static int CALLBACK traitsInFamilyEnumProc(CONST LOGFONT* logFont, CONST TEXTMETRIC* metrics, DWORD fontType, LPARAM lParam)
{
    TraitsInFamilyProcData* procData = reinterpret_cast<TraitsInFamilyProcData*>(lParam);

    unsigned traitsMask = 0;
    traitsMask |= logFont->lfItalic ? FontStyleItalicMask : FontStyleNormalMask;
    traitsMask |= FontVariantNormalMask;
    LONG weight = logFont->lfWeight;
    traitsMask |= weight == FW_THIN ? FontWeight100Mask :
        weight == FW_EXTRALIGHT ? FontWeight200Mask :
        weight == FW_LIGHT ? FontWeight300Mask :
        weight == FW_NORMAL ? FontWeight400Mask :
        weight == FW_MEDIUM ? FontWeight500Mask :
        weight == FW_SEMIBOLD ? FontWeight600Mask :
        weight == FW_BOLD ? FontWeight700Mask :
        weight == FW_EXTRABOLD ? FontWeight800Mask :
                                 FontWeight900Mask;
    procData->m_traitsMasks.add(traitsMask);
    return 1;
}

void FontCache::getTraitsInFamily(const AtomicString& familyName, Vector<unsigned>& traitsMasks)
{
    HDC hdc = GetDC(0);

    LOGFONT logFont;
    logFont.lfCharSet = DEFAULT_CHARSET;
    unsigned familyLength = min(familyName.length(), static_cast<unsigned>(LF_FACESIZE - 1));
    memcpy(logFont.lfFaceName, familyName.characters(), familyLength * sizeof(UChar));
    logFont.lfFaceName[familyLength] = 0;
    logFont.lfPitchAndFamily = 0;

    TraitsInFamilyProcData procData(familyName);
    EnumFontFamiliesEx(hdc, &logFont, traitsInFamilyEnumProc, reinterpret_cast<LPARAM>(&procData), 0);
    copyToVector(procData.m_traitsMasks, traitsMasks);

    ReleaseDC(0, hdc);
}

FontPlatformData* FontCache::createFontPlatformData(const FontDescription& fontDescription, const AtomicString& family)
{
    LOGFONT winfont = {0};
    FillLogFont(fontDescription, &winfont);

    // Windows will always give us a valid pointer here, even if the face name 
    // is non-existent.  We have to double-check and see if the family name was 
    // really used.
    String winName;
    HFONT hfont = createFontIndirectAndGetWinName(family, &winfont, &winName);
    if (!hfont)
        return 0;

    // TODO(pamg): Do we need to use predefined fonts "guaranteed" to exist
    // when we're running in layout-test mode?
    if (!equalIgnoringCase(family, winName)) {
        // For CJK fonts with both English and native names, 
        // GetTextFace returns a native name under the font's "locale"
        // and an English name under other locales regardless of 
        // lfFaceName field of LOGFONT. As a result, we need to check
        // if a font has an alternate name. If there is, we need to
        // compare it with what's requested in the first place.
        String altName;
        if (!LookupAltName(family, altName) || 
            !equalIgnoringCase(altName, winName)) {
            DeleteObject(hfont);
            return 0;
        }
    }

    return new FontPlatformData(hfont,
                                fontDescription.computedPixelSize());
}

}
