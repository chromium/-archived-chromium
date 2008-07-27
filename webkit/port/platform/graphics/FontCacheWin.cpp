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
#include "FontMetrics.h"
#include "Font.h"
#include "SimpleFontData.h"
#include <algorithm>
#include <hash_map>
#include <string>
#include <windows.h>
#include <mlang.h>

#include "base/gfx/font_utils.h"
#include "webkit/glue/webkit_glue.h"

using std::min;

namespace WebCore
{

void FontCache::platformInit()
{
    // Not needed on Windows.
}

// FIXME(jungshik) : consider adding to WebKit String class
static bool IsStringASCII(const String& s)
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
    struct fontCodepage {
        WCHAR *name;
        int   codePage;
    };

    struct namePair {
        WCHAR *name;
        fontCodepage altNameCp; 
    };

    // FIXME(jungshik) : This list probably covers 99% of cases. 
    // To cover the remaining 1% and cut down the file size, 
    // consider accessing 'NAME' table of a truetype font
    // using |GetFontData| and caching the mapping. 
    // 932 : Japanese, 936 : Simp. Chinese, 949 : Korean, 950 : Trad. Chinese
    // In the table below, the ASCII keys are all lower-cased for
    // case-insensitive matching.
    static const namePair  namePairs[] = {
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
    };

    typedef stdext::hash_map<std::wstring, const fontCodepage*> nameMap;
    static nameMap* fontNameMap = NULL;

    if (!fontNameMap) {
        size_t numElements = sizeof(namePairs) / sizeof(namePair);
        fontNameMap = new nameMap;
        for (size_t i = 0; i < numElements; ++i) {
            (*fontNameMap)[std::wstring(namePairs[i].name)] =
                &(namePairs[i].altNameCp);
        }
    }

    bool isAscii = false; 
    std::wstring n;
    // use |lower| only for ASCII names 
    // For non-ASCII names, we don't want to invoke an expensive 
    // and unnecessary |lower|. 
    if (IsStringASCII(name)) {
        isAscii = true;
        n.assign(name.lower().characters(), name.length());
    } else {
        n.assign(name.characters(), name.length());
    }

    nameMap::const_iterator iter = fontNameMap->find(n);
    if (iter == fontNameMap->end()) {
        return false;
    }

    static int systemCp = ::GetACP();
    int fontCp = iter->second->codePage;

    if ((isAscii && systemCp == fontCp) ||
        (!isAscii && systemCp != fontCp)) {
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

IMLangFontLink2* FontCache::getFontLinkInterface()
{
  return webkit_glue::GetLangFontLink();
}

// Given the desired base font, this will create a SimpleFontData for a specific
// font that can be used to render the given range of characters.
// Two methods are used : our own getFallbackFamily and Windows' font linking.
// IMLangFontLink will give us a range of characters, and may not find a font
// that matches all the input characters. However, normally, we will only get
// called with one input character because this is used to find a glyph for
// a missing one. If we are called for a range, I *believe* that it will be
// used to populate the glyph cache only, meaning we will be called again for
// the next missing glyph.
const SimpleFontData* FontCache::getFontDataForCharacters(const Font& font,
                                                    const UChar* characters,
                                                    int length)
{
    // Use Safari mac's font-fallback mechanism when in layout test mode.
    if (webkit_glue::IsLayoutTestMode() && length == 1) {
        // Get the family name for the font
        String origFamily =
            font.primaryFont()->platformData().overrideFontMetrics()->family;

        const String* fallbackFamily =
            FontFallbackMetrics::lookup(origFamily, characters[0]);

        if (fallbackFamily) {
            FontPlatformData* platformData = getCachedFontPlatformData(
                font.fontDescription(), *fallbackFamily);
            if (platformData) {
                return new SimpleFontData(*platformData);
            }
        }
    }

    // TODO(jungshik) : Consider passing fontDescription.dominantScript() 
    // to GetFallbackFamily here along with the corresponding change
    // in base/gfx.
    FontDescription fontDescription = font.fontDescription();
    const wchar_t* family = gfx::GetFallbackFamily(characters, length,
        static_cast<gfx::GenericFamilyType>(fontDescription.genericFamily()));
    FontPlatformData* data = NULL;
    if (family) {
        data = getCachedFontPlatformData(font.fontDescription(), 
                                         AtomicString(family, wcslen(family)),
                                         false); 
    }

    // last resort font list : PanUnicode
    const static char* const panUniFonts[] = {
        "Arial Unicode MS",
        "Bitstream Cyberbit", 
        "Code2000",
        "Titus Cyberbit Basic",
        "Microsoft Sans Serif",
        "Lucida Sans Unicode"
    };
    for (int i = 0; !data && i < ARRAYSIZE(panUniFonts); ++i) {
        data = getCachedFontPlatformData(font.fontDescription(),
                                         panUniFonts[i]); 
    }
    if (data) 
       return new SimpleFontData(*data);

    // IMLangFontLink can break up a string into regions that can be rendered
    // using one particular font.
    // See http://blogs.msdn.com/oldnewthing/archive/2004/07/16/185261.aspx
    IMLangFontLink2* langFontLink = getFontLinkInterface();
    if (!langFontLink)
        return 0;

    SimpleFontData* fontData = 0;
    HDC hdc = GetDC(0);
    HFONT primaryFont = font.primaryFont()->fontDataForCharacter(characters[0])->m_font.hfont();

    // Get the code pages supported by the requested font.
    DWORD acpCodePages;
    langFontLink->CodePageToCodePages(CP_ACP, &acpCodePages);

    // Get the code pages required by the given string, passing in the font's
    // code pages as the ones to give "priority". Priority code pages will be
    // used if there are multiple code pages that can represent the characters
    // in question, and of course, we want to use a page supported by the
    // primary font if possible.
    DWORD actualCodePages;
    long cchActual;
    if (FAILED(langFontLink->GetStrCodePages(characters, length, acpCodePages,
                                             &actualCodePages, &cchActual)))
        cchActual = 0;

    if (cchActual) {
        // GetStrCodePages has found a sequence of characters that can be
        // represented in one font. MapFont will create this mystical font.
        HFONT result;
        // FIXME(jungshik) To make MapFont inherit the properties from 
        // the current font, the current font needs to be selected into DC. 
        // However, that leads to a failure in intl page cycler, 
        // a slow rendering and issue 735750. We need to implement a 
        // real font fallback at a higher level (issue 698618)
        if (langFontLink->MapFont(hdc, actualCodePages, characters[0], &result) == S_OK) {
            // This font will have to be deleted using the IMLangFontLink2
            // rather than the normal way.
            fontData = new SimpleFontData(FontPlatformData(result, 0, 0, true));
        }
    }

    ReleaseDC(0, hdc);
    return fontData;
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
    if (webkit_glue::IsLayoutTestMode())
        return getCachedFontPlatformData(description, AtomicString("Times"));

    FontDescription::GenericFamilyType generic = description.genericFamily();
    // TODO(jungshik): Mapping webkit generic to gfx::GenericFamilyType needs to be
    // more intelligent and the mapping function should be added to webkit_glue. 
    // This spot rarely gets reached. GetFontDataForCharacters() gets hit a lot
    // more often (see TODO comment there). 
    const wchar_t* family = gfx::GetFontFamilyForScript(description.dominantScript(),
        static_cast<gfx::GenericFamilyType>(generic));

    if (family) 
      return getCachedFontPlatformData(description, AtomicString(family, wcslen(family)));

    // FIXME: Would be even better to somehow get the user's default font here.  For now we'll pick
    // the default that the user would get without changing any prefs.
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

// TODO(jungshik): This may not be the best place to put this function. See
// TODO in pending/FontCache.h.
AtomicString FontCache::getGenericFontForScript(UScriptCode script, const FontDescription& description)
{
    FontDescription::GenericFamilyType generic = description.genericFamily();
    const wchar_t* scriptFont = gfx::GetFontFamilyForScript(
        script, static_cast<gfx::GenericFamilyType>(generic));
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
    winfont->lfQuality = webkit_glue::IsLayoutTestMode() ? NONANTIALIASED_QUALITY
        : DEFAULT_QUALITY; // Honor user's desktop settings.
    winfont->lfPitchAndFamily = DEFAULT_PITCH | FF_DONTCARE;
    winfont->lfItalic = fontDescription.italic();

    // FIXME: Support weights for real.  Do our own enumeration of the available weights.
    // We can't rely on Windows here, since we need to follow the CSS2 algorithm for how to fill in
    // gaps in the weight list.
    // fontExists() used to hardcod Lucida Grande. According to FIXME comment,
    // that's because it uses different weights than typical Win32 fonts
    // (500/600 instead of 400/700). However, createFontPlatformData
    // didn't. Special-casing Lucida Grande in a refactored function
    // led to massive webkit test failure. 
    winfont->lfWeight = fontDescription.bold() ? 700 : 400;
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

    const FontMetrics* overrideFontMetrics = NULL;

    if (webkit_glue::IsLayoutTestMode()) {
        // In layout-test mode, we have a font IFF it exists in our font size
        // cache.  We want to ignore the existence/absence of the font in the
        // system.
        overrideFontMetrics = FontMetrics::lookup(family,
                                                  fontDescription.bold(),
                                                  fontDescription.italic());
        if (!overrideFontMetrics) {
            DeleteObject(hfont);
            return 0;
        }
    } else if (!equalIgnoringCase(family, winName)) {
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
                                fontDescription.computedPixelSize(),
                                overrideFontMetrics,
                                false);
}

}

