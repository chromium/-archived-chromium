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

#include "base/gfx/font_utils.h"

#include <limits>
#include <map>

#include "base/gfx/uniscribe.h"
#include "base/logging.h"
#include "base/singleton.h"
#include "base/string_util.h"
#include "unicode/locid.h"

namespace gfx {

namespace {

// hash_map has extra cost with no sizable gain for a small number of integer
// key items. When the map size becomes much bigger (which will be later as
// more scripts are added) and this turns out to be prominent in the profile, we
// may consider switching to hash_map (or just an array if we support all the
// scripts)
typedef std::map<UScriptCode, const wchar_t*> ScriptToFontMap;

struct ScriptToFontMapSingletonTraits
    : public DefaultSingletonTraits<ScriptToFontMap> {
  static ScriptToFontMap* New() {
    struct FontMap {
      UScriptCode script;
      const wchar_t* family;
    };

    const static FontMap font_map[] = {
      {USCRIPT_SIMPLIFIED_HAN, L"simsun"},
      {USCRIPT_TRADITIONAL_HAN, L"pmingliu"},
      {USCRIPT_HIRAGANA, L"ms pgothic"},
      {USCRIPT_KATAKANA, L"ms pgothic"},
      {USCRIPT_KATAKANA_OR_HIRAGANA, L"ms pgothic"},
      {USCRIPT_HANGUL, L"gulim"},
      {USCRIPT_THAI, L"tahoma"},
      {USCRIPT_HEBREW, L"david"},
      {USCRIPT_ARABIC, L"simplified arabic"},
      {USCRIPT_DEVANAGARI, L"mangal"},
      {USCRIPT_BENGALI, L"vrinda"},
      {USCRIPT_GURMUKHI, L"raavi"},
      {USCRIPT_GUJARATI, L"shruti"},
      {USCRIPT_ORIYA, L"kalinga"},
      {USCRIPT_TAMIL, L"latha"},
      {USCRIPT_TELUGU, L"gautami"},
      {USCRIPT_KANNADA, L"tunga"},
      {USCRIPT_MALAYALAM, L"kartika"},
      {USCRIPT_LAO, L"dokchampa"},
      {USCRIPT_TIBETAN, L"microsoft himalaya"},
      {USCRIPT_GEORGIAN, L"sylfaen"},
      {USCRIPT_ARMENIAN, L"sylfaen"},
      {USCRIPT_ETHIOPIC, L"nyala"},
      {USCRIPT_CANADIAN_ABORIGINAL, L"euphemia"},
      {USCRIPT_CHEROKEE, L"plantagenet cherokee"},
      {USCRIPT_YI, L"microsoft yi balti"},
      {USCRIPT_SINHALA, L"iskoola pota"},
      {USCRIPT_SYRIAC, L"estrangelo edessa"},
      {USCRIPT_KHMER, L"daunpenh"},
      {USCRIPT_THAANA, L"mv boli"},
      {USCRIPT_MONGOLIAN, L"mongolian balti"},
      // For common, perhaps we should return a font
      // for the current application/system locale.
      //{USCRIPT_COMMON, L"times new roman"}
    };

    ScriptToFontMap* new_instance = new ScriptToFontMap;
    // Cannot recover from OOM so that there's no need to check.
    for (int i = 0; i < arraysize(font_map); ++i)
      (*new_instance)[font_map[i].script] = font_map[i].family;

    // Initialize the locale-dependent mapping.
    // Since Chrome synchronizes the ICU default locale with its UI locale,
    // this ICU locale tells the current UI locale of Chrome.
    Locale locale = Locale::getDefault();
    ScriptToFontMap::const_iterator iter;
    if (locale == Locale::getKorean()) {
      iter = new_instance->find(USCRIPT_HANGUL);
    } else if (locale == Locale::getJapanese()) {
      iter = new_instance->find(USCRIPT_KATAKANA_OR_HIRAGANA);
    } else if (locale == Locale::getTraditionalChinese()) {
      iter = new_instance->find(USCRIPT_TRADITIONAL_HAN);
    } else {
      iter = new_instance->find(USCRIPT_SIMPLIFIED_HAN);
    }
    if (iter != new_instance->end())
      (*new_instance)[USCRIPT_HAN] = iter->second;

    return new_instance;
  }
};

Singleton<ScriptToFontMap, ScriptToFontMapSingletonTraits> script_font_map;

const int kUndefinedAscent = std::numeric_limits<int>::min();

// Given an HFONT, return the ascent. If GetTextMetrics fails,
// kUndefinedAscent is returned, instead.
int GetAscent(HFONT hfont) {
  HDC dc = GetDC(NULL);
  HGDIOBJ oldFont = SelectObject(dc, hfont);
  TEXTMETRIC tm;
  BOOL got_metrics = GetTextMetrics(dc, &tm);
  SelectObject(dc, oldFont);
  ReleaseDC(NULL, dc);
  return got_metrics ? tm.tmAscent : kUndefinedAscent;
}

struct FontData {
  FontData() : hfont(NULL), ascent(kUndefinedAscent), script_cache(NULL) {}
  HFONT hfont;
  int ascent;
  mutable SCRIPT_CACHE script_cache;
};

// Again, using hash_map does not earn us much here.
// page_cycler_test intl2 gave us a 'better' result with map than with hash_map
// even though they're well-within 1-sigma of each other so that the difference
// is not significant. On the other hand, some pages in intl2 seem to
// take longer to load with map in the 1st pass. Need to experiment further.
typedef std::map<std::wstring, FontData*> FontDataCache;
struct FontDataCacheSingletonTraits
    : public DefaultSingletonTraits<FontDataCache> {
  static void Delete(FontDataCache* cache) {
    FontDataCache::iterator iter = cache->begin();
    while (iter != cache->end()) {
      SCRIPT_CACHE script_cache = iter->second->script_cache;
      if (script_cache)
        ScriptFreeCache(&script_cache);
      delete iter->second;
      ++iter;
    }
    delete cache;
  }
};

}  // namespace

// TODO(jungshik) : this is font fallback code version 0.1
//  - Cover all the scripts
//  - Get the default font for each script/generic family from the
//    preference instead of hardcoding in the source.
//  - Support generic families (from FontDescription)
//  - If the default font for a script is not available,
//    use EnumFontFamilies or similar APIs to come up with a list of
//    fonts supporting the script and cache the result.
//  - Consider using UnicodeSet (or UnicodeMap) to keep track of which
//    character is supported by which font
//  - Update script_font_cache in response to WM_FONTCHANGE

const wchar_t* GetFontFamilyForScript(UScriptCode script,
                                      GenericFamilyType generic) {
  ScriptToFontMap::const_iterator iter = script_font_map->find(script);
  const wchar_t* family = NULL;
  if (iter != script_font_map->end()) {
    family = iter->second;
  }
  return family;
}

// TODO(jungshik)
//  - Handle 'Inherited', 'Common' and 'Unknown'
//    (see http://www.unicode.org/reports/tr24/#Usage_Model )
//    For 'Inherited' and 'Common', perhaps we need to
//    accept another parameter indicating the previous family
//    and just return it.
//  - All the characters (or characters up to the point a single
//    font can cover) need to be taken into account
const wchar_t* GetFallbackFamily(const wchar_t* characters,
                                 int length,
                                 GenericFamilyType generic) {
  DCHECK(characters && characters[0] && length > 0);
  UScriptCode script = USCRIPT_COMMON;

  // Sometimes characters common to script (e.g. space) is at
  // the beginning of a string so that we need to skip them
  // to get a font required to render the string.
  int i = 0;
  UChar32 ucs4 = 0;
  while (i < length && script == USCRIPT_COMMON ||
         script == USCRIPT_INVALID_CODE) {
    U16_NEXT(characters, i, length, ucs4);
    UErrorCode err = U_ZERO_ERROR;
    script = uscript_getScript(ucs4, &err);
    // silently ignore the error
  }

  // hack for full width ASCII. Japanese are most fond of full-width ASCII.
  // TODO(jungshik) find a better way !
  if (0xFF00 < ucs4 && ucs4 < 0xFF5F)
    return L"ms pgothic";

  const wchar_t* family = GetFontFamilyForScript(script, generic);
  if (!family) {
    int plane = ucs4 >> 16;
    switch (plane) {
      case 1:
        family = L"code2001";
        break;
      case 2:
        family = L"simsun ext b";
        break;
      default:
        family = L"arial unicode ms";
    }
  }

  return family;
}



// Be aware that this is not thread-safe.
bool GetDerivedFontData(const wchar_t *family,
                        int style,
                        LOGFONT *logfont,
                        int *ascent,
                        HFONT *hfont,
                        SCRIPT_CACHE **script_cache) {
  DCHECK(logfont && family && *family);
  // Using |Singleton| here is not free, but the intl2 page cycler test
  // does not show any noticeable difference with and without it. Leaking
  // the contents of FontDataCache (especially SCRIPT_CACHE) at the end
  // of a renderer process may not be a good idea. We may use
  // atexit(). However, with no noticeable performance difference, |Singleton|
  // is cleaner, I believe.
  FontDataCache* font_data_cache =
      Singleton<FontDataCache, FontDataCacheSingletonTraits>::get();
  // TODO(jungshik) : This comes up pretty high in the profile so that
  // we need to measure whether using SHA256 (after coercing all the
  // fields to char*) is faster than StringPrintf.
  std::wstring font_key = StringPrintf(L"%1d:%d:%s", style, logfont->lfHeight,
                                       family);
  FontDataCache::const_iterator iter = font_data_cache->find(font_key);
  FontData *derived;
  if (iter == font_data_cache->end()) {
    DCHECK(wcslen(family) < LF_FACESIZE);
    wcscpy_s(logfont->lfFaceName, LF_FACESIZE, family);
    // TODO(jungshik): CreateFontIndirect always comes up with
    // a font even if there's no font matching the name. Need to
    // check it against what we actually want (as is done in FontCacheWin.cpp)
    derived = new FontData;
    derived->hfont = CreateFontIndirect(logfont);
    // GetAscent may return kUndefinedAscent, but we still want to
    // cache it so that we won't have to call CreateFontIndirect once
    // more for HFONT next time.
    derived->ascent = GetAscent(derived->hfont);
    (*font_data_cache)[font_key] = derived;
  } else {
    derived = iter->second;
    // Last time, GetAscent failed so that only HFONT was
    // cached. Try once more assuming that TryPreloadFont
    // was called by a caller between calls.
    if (kUndefinedAscent == derived->ascent)
      derived->ascent = GetAscent(derived->hfont);
  }
  *hfont = derived->hfont;
  *ascent = derived->ascent;
  *script_cache = &(derived->script_cache);
  return *ascent != kUndefinedAscent;
}

int GetStyleFromLogfont(const LOGFONT* logfont) {
  // TODO(jungshik) : consider defining UNDEFINED or INVALID for style and
  //                  returning it when logfont is NULL
  if (!logfont) {
    NOTREACHED();
    return FONT_STYLE_NORMAL;
  }
  return (logfont->lfItalic ? FONT_STYLE_ITALIC : FONT_STYLE_NORMAL) |
         (logfont->lfUnderline ? FONT_STYLE_UNDERLINED : FONT_STYLE_NORMAL) |
         (logfont->lfWeight >= 700 ? FONT_STYLE_BOLD : FONT_STYLE_NORMAL);
}

}  // namespace gfx
