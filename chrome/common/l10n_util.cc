// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"

#include "chrome/common/l10n_util.h"

#include "base/command_line.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "base/scoped_ptr.h"
#include "base/string16.h"
#include "base/string_piece.h"
#include "base/string_util.h"
#include "base/sys_string_conversions.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/gfx/chrome_canvas.h"
#include "chrome/common/resource_bundle.h"
#include "unicode/uscript.h"

// TODO(playmobil): remove this undef once SkPostConfig.h is fixed.
// skia/include/corecg/SkPostConfig.h #defines strcasecmp() so we can't use
// base::strcasecmp() without #undefing it here.
#undef strcasecmp

namespace {

#if defined(OS_WIN)
static const FilePath::CharType kLocaleFileExtension[] = L".dll";
#elif defined(OS_POSIX)
static const FilePath::CharType kLocaleFileExtension[] = ".pak";
#endif

// Added to the end of strings that are too big in TrucateString.
static const wchar_t* const kElideString = L"\x2026";

// Get language and region from the OS.
void GetLanguageAndRegionFromOS(std::string* lang, std::string* region) {
  // Later we may have to change this to be OS-dependent so that
  // it's not affected by ICU's default locale. It's all right
  // to do this way because SetICUDefaultLocale is internal
  // to this file and we know where/when it's called.
  Locale locale = Locale::getDefault();
  const char* language = locale.getLanguage();
  const char* country = locale.getCountry();
  DCHECK(language);
  *lang = language;
  *region = country;
}

// Convert Chrome locale name to ICU locale name
std::string ICULocaleName(const std::wstring& locale_string) {
  // If not Spanish, just return it.
  if (locale_string.substr(0, 2) != L"es")
    return WideToASCII(locale_string);
  // Expand es to es-ES.
  if (LowerCaseEqualsASCII(locale_string, "es"))
    return "es-ES";
  // Map es-419 (Latin American Spanish) to es-FOO depending on the system
  // locale.  If it's es-RR other than es-ES, map to es-RR. Otherwise, map
  // to es-MX (the most populous in Spanish-speaking Latin America).
  if (LowerCaseEqualsASCII(locale_string, "es-419")) {
    std::string lang, region;
    GetLanguageAndRegionFromOS(&lang, &region);
    if (LowerCaseEqualsASCII(lang, "es") &&
        !LowerCaseEqualsASCII(region, "es")) {
      lang.append("-");
      lang.append(region);
      return lang;
    }
    return "es-MX";
  }
  // Currently, Chrome has only "es" and "es-419", but later we may have
  // more specific "es-RR".
  return WideToASCII(locale_string);
}

// Sets the default locale of ICU.
// When the application locale (UI locale) of Chrome is specified with
// '--lang' command line flag or 'intl.app_locale' entry in the "Preferences",
// the default locale of ICU need to be changed to match the application locale
// so that ICU functions work correctly in a locale-dependent manner.
// This is handy in that we don't have to call GetApplicationLocale()
// everytime we call locale-dependent ICU APIs as long as we make sure
// that this is called before any locale-dependent API is called.
UBool SetICUDefaultLocale(const std::wstring& locale_string) {
  Locale locale(ICULocaleName(locale_string).c_str());
  UErrorCode error_code = U_ZERO_ERROR;
  Locale::setDefault(locale, error_code);
  // This return value is actually bogus because Locale object is
  // an ID and setDefault seems to always succeed (regardless of the
  // presence of actual locale data). However,
  // it does not hurt to have it as a sanity check.
  return U_SUCCESS(error_code);
}

// Returns true if |locale_name| has an alias in the ICU data file.
bool IsDuplicateName(const std::string& locale_name) {
  static const char* const kDuplicateNames[] = {
    "en",
    "pt",
    "zh",
    "zh_hans_cn",
    "zh_hant_tw"
  };

  // Skip all 'es_RR'. Currently, we use 'es' for es-ES (Spanish in Spain).
  // 'es-419' (Spanish in Latin America) is not available in ICU so that it
  // has to be added manually in GetAvailableLocales().
  if (LowerCaseEqualsASCII(locale_name.substr(0, 3),  "es_"))
    return true;
  for (size_t i = 0; i < arraysize(kDuplicateNames); ++i) {
    if (base::strcasecmp(kDuplicateNames[i], locale_name.c_str()) == 0)
      return true;
  }
  return false;
}

bool IsLocaleAvailable(const std::wstring& locale,
                       const std::wstring& locale_path) {
  std::wstring test_locale = locale;
  // If locale has any illegal characters in it, we don't want to try to
  // load it because it may be pointing outside the locale data file directory.
  file_util::ReplaceIllegalCharacters(&test_locale, ' ');
  if (test_locale != locale)
    return false;

  if (!l10n_util::IsLocaleSupportedByOS(locale))
    return false;

  FilePath test_path = FilePath::FromWStringHack(locale_path)
      .Append(FilePath::FromWStringHack(locale))
      .ReplaceExtension(kLocaleFileExtension);
  return file_util::PathExists(test_path) && SetICUDefaultLocale(locale);
}

bool CheckAndResolveLocale(const std::wstring& locale,
                           const std::wstring& locale_path,
                           std::wstring* resolved_locale) {
  if (IsLocaleAvailable(locale, locale_path)) {
    *resolved_locale = locale;
    return true;
  }
  // If the locale matches language but not country, use that instead.
  // TODO(jungshik) : Nothing is done about languages that Chrome
  // does not support but available on Windows. We fall
  // back to en-US in GetApplicationLocale so that it's a not critical,
  // but we can do better.
  std::wstring::size_type hyphen_pos = locale.find(L'-');
  if (hyphen_pos != std::wstring::npos && hyphen_pos > 0) {
    std::wstring lang(locale, 0, hyphen_pos);
    std::wstring region(locale, hyphen_pos + 1);
    std::wstring tmp_locale(lang);
    // Map es-RR other than es-ES to es-419 (Chrome's Latin American
    // Spanish locale).
    if (LowerCaseEqualsASCII(lang, "es") && !LowerCaseEqualsASCII(region, "es"))
      tmp_locale.append(L"-419");
    else if (LowerCaseEqualsASCII(lang, "zh")) {
      // Map zh-HK and zh-MK to zh-TW. Otherwise, zh-FOO is mapped to zh-CN.
     if (LowerCaseEqualsASCII(region, "hk") ||
         LowerCaseEqualsASCII(region, "mk")) {
       tmp_locale.append(L"-TW");
     } else {
       tmp_locale.append(L"-CN");
     }
    }
    if (IsLocaleAvailable(tmp_locale, locale_path)) {
      resolved_locale->swap(tmp_locale);
      return true;
    }
  }

  // Google updater uses no, iw and en for our nb, he, and en-US.
  // We need to map them to our codes.
  struct {
    const char* source;
    const wchar_t* dest;} alias_map[] = {
      {"no", L"nb"},
      {"tl", L"fil"},
      {"iw", L"he"},
      {"en", L"en-US"},
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(alias_map); ++i) {
    if (LowerCaseEqualsASCII(locale, alias_map[i].source)) {
      std::wstring tmp_locale(alias_map[i].dest);
      if (IsLocaleAvailable(tmp_locale, locale_path)) {
        resolved_locale->swap(tmp_locale);
        return true;
      }
    }
  }

  return false;
}

// Get the locale of the operating system.  The return value is of the form
// language[-country] (e.g., en-US) where the language is the 2 letter code from
// ISO-639.
std::wstring GetSystemLocale() {
  std::string language, region;
  GetLanguageAndRegionFromOS(&language, &region);
  std::string ret;
  if (!language.empty())
    ret.append(language);
  if (!region.empty()) {
    ret.append("-");
    ret.append(region);
  }
  return ASCIIToWide(ret);
}

}  // namespace

namespace l10n_util {

// Represents the locale-specific text direction.
static TextDirection g_text_direction = UNKNOWN_DIRECTION;

std::wstring GetApplicationLocale(const std::wstring& pref_locale) {
#if defined(OS_MACOSX)
  // On the mac, we don't want to test preferences or ICU for the language,
  // we want to use whatever Cocoa is using when it loaded the main nib file.
  // It handles all the mapping and fallbacks for us, we just need to ask
  // Cocoa.
  // TODO(pinkerton): break this out into a .mm and ask Cocoa.
  return L"en";
#else
  std::wstring locale_path;
  PathService::Get(chrome::DIR_LOCALES, &locale_path);
  std::wstring resolved_locale;

  // First, check to see if there's a --lang flag.
  const CommandLine& parsed_command_line = *CommandLine::ForCurrentProcess();
  const std::wstring& lang_arg =
      parsed_command_line.GetSwitchValue(switches::kLang);
  if (!lang_arg.empty()) {
    if (CheckAndResolveLocale(lang_arg, locale_path, &resolved_locale))
      return resolved_locale;
  }

  // Second, try user prefs.
  if (!pref_locale.empty()) {
    if (CheckAndResolveLocale(pref_locale, locale_path, &resolved_locale))
      return resolved_locale;
  }

  // Next, try the system locale.
  const std::wstring system_locale = GetSystemLocale();
  if (CheckAndResolveLocale(system_locale, locale_path, &resolved_locale))
    return resolved_locale;

  // Fallback on en-US.
  const std::wstring fallback_locale(L"en-US");
  if (IsLocaleAvailable(fallback_locale, locale_path))
    return fallback_locale;

  // No locale data file was found; we shouldn't get here.
  NOTREACHED();

  return std::wstring();
#endif
}

std::wstring GetLocalName(const std::string& locale_code_str,
                          const std::wstring& app_locale_wstr,
                          bool is_for_ui) {
  const std::string app_locale = WideToASCII(app_locale_wstr);
  const char* locale_code = locale_code_str.c_str();
  UErrorCode error = U_ZERO_ERROR;
  const int buffer_size = 1024;

#if defined(WCHAR_T_IS_UTF32)
  string16 name_local_utf16;
  int actual_size = uloc_getDisplayName(locale_code, app_locale.c_str(),
      WriteInto(&name_local_utf16, buffer_size + 1), buffer_size, &error);
  std::wstring name_local = UTF16ToWide(name_local_utf16);
#else
  std::wstring name_local;
  int actual_size = uloc_getDisplayName(locale_code, app_locale.c_str(),
      WriteInto(&name_local, buffer_size + 1), buffer_size, &error);
#endif
  DCHECK(U_SUCCESS(error));
  name_local.resize(actual_size);
  // Add an RTL mark so parentheses are properly placed.
  if (is_for_ui && GetTextDirection() == RIGHT_TO_LEFT) {
    name_local.push_back(static_cast<wchar_t>(kRightToLeftMark));
  }
  return name_local;
}

std::wstring GetString(int message_id) {
  ResourceBundle &rb = ResourceBundle::GetSharedInstance();
  return rb.GetLocalizedString(message_id);
}

static std::wstring GetStringF(int message_id,
                               const std::wstring& a,
                               const std::wstring& b,
                               const std::wstring& c,
                               const std::wstring& d,
                               std::vector<size_t>* offsets) {
  const std::wstring& format_string = GetString(message_id);
  std::wstring formatted = ReplaceStringPlaceholders(format_string, a, b, c,
                                                     d, offsets);
  return formatted;
}

std::wstring GetStringF(int message_id,
                        const std::wstring& a) {
  return GetStringF(message_id, a, std::wstring(), std::wstring(),
                    std::wstring(), NULL);
}

std::wstring GetStringF(int message_id,
                        const std::wstring& a,
                        const std::wstring& b) {
  return GetStringF(message_id, a, b, std::wstring(), std::wstring(), NULL);
}

std::wstring GetStringF(int message_id,
                        const std::wstring& a,
                        const std::wstring& b,
                        const std::wstring& c) {
  return GetStringF(message_id, a, b, c, std::wstring(), NULL);
}

std::wstring GetStringF(int message_id,
                        const std::wstring& a,
                        size_t* offset) {
  DCHECK(offset);
  std::vector<size_t> offsets;
  std::wstring result = GetStringF(message_id, a, std::wstring(),
                                   std::wstring(), std::wstring(), &offsets);
  DCHECK(offsets.size() == 1);
  *offset = offsets[0];
  return result;
}

std::wstring GetStringF(int message_id,
                        const std::wstring& a,
                        const std::wstring& b,
                        std::vector<size_t>* offsets) {
  return GetStringF(message_id, a, b, std::wstring(), std::wstring(), offsets);
}

std::wstring GetStringF(int message_id, int a) {
  return GetStringF(message_id, IntToWString(a));
}

std::wstring GetStringF(int message_id, int64 a) {
  return GetStringF(message_id, Int64ToWString(a));
}

std::wstring TruncateString(const std::wstring& string, size_t length) {
  if (string.size() <= length)
    // String fits, return it.
    return string;

  if (length == 0) {
    // No room for the ellide string, return an empty string.
    return std::wstring(L"");
  }
  size_t max = length - 1;

  if (max == 0) {
    // Just enough room for the elide string.
    return kElideString;
  }

#if defined(WCHAR_T_IS_UTF32)
  const string16 string_utf16 = WideToUTF16(string);
#else
  const std::wstring &string_utf16 = string;
#endif
  // Use a line iterator to find the first boundary.
  UErrorCode status = U_ZERO_ERROR;
  scoped_ptr<RuleBasedBreakIterator> bi(static_cast<RuleBasedBreakIterator*>(
      RuleBasedBreakIterator::createLineInstance(Locale::getDefault(),
                                                 status)));
  if (U_FAILURE(status))
    return string.substr(0, max) + kElideString;
  bi->setText(string_utf16.c_str());
  int32_t index = bi->preceding(static_cast<int32_t>(max));
  if (index == BreakIterator::DONE) {
    index = static_cast<int32_t>(max);
  } else {
    // Found a valid break (may be the beginning of the string). Now use
    // a character iterator to find the previous non-whitespace character.
    StringCharacterIterator char_iterator(string_utf16.c_str());
    if (index == 0) {
      // No valid line breaks. Start at the end again. This ensures we break
      // on a valid character boundary.
      index = static_cast<int32_t>(max);
    }
    char_iterator.setIndex(index);
    while (char_iterator.hasPrevious()) {
      char_iterator.previous();
      if (!(u_isspace(char_iterator.current()) ||
            u_charType(char_iterator.current()) == U_CONTROL_CHAR ||
            u_charType(char_iterator.current()) == U_NON_SPACING_MARK)) {
        // Not a whitespace character. Advance the iterator so that we
        // include the current character in the truncated string.
        char_iterator.next();
        break;
      }
    }
    if (char_iterator.hasPrevious()) {
      // Found a valid break point.
      index = char_iterator.getIndex();
    } else {
      // String has leading whitespace, return the elide string.
      return kElideString;
    }
  }
  return string.substr(0, index) + kElideString;
}

#if defined(WCHAR_T_IS_UTF32)
std::wstring ToLower(const std::wstring& string) {
  string16 string_utf16 = WideToUTF16(string);
  UnicodeString lower_u_str(
      UnicodeString(string_utf16.c_str()).toLower(Locale::getDefault()));
  string16 result_utf16;
  lower_u_str.extract(0, lower_u_str.length(),
                      WriteInto(&result_utf16, lower_u_str.length() + 1));
  std::wstring result = UTF16ToWide(result_utf16);
  return result;
}
#else
std::wstring ToLower(const std::wstring& string) {
  UnicodeString lower_u_str(
      UnicodeString(string.c_str()).toLower(Locale::getDefault()));
  std::wstring result;
  lower_u_str.extract(0, lower_u_str.length(),
                      WriteInto(&result, lower_u_str.length() + 1));
  return result;
}
#endif  // defined(WCHAR_T_IS_UTF32)

// Returns the text direction for the default ICU locale. It is assumed
// that SetICUDefaultLocale has been called to set the default locale to
// the UI locale of Chrome.
TextDirection GetTextDirection() {
  if (g_text_direction == UNKNOWN_DIRECTION) {
    const Locale& locale = Locale::getDefault();
    g_text_direction = GetTextDirectionForLocale(locale.getName());
  }
  return g_text_direction;
}

TextDirection GetTextDirectionForLocale(const char* locale_name) {
  UScriptCode scripts[10]; // 10 scripts should be enough for any locale.
  UErrorCode error = U_ZERO_ERROR;
  int n = uscript_getCode(locale_name, scripts, 10, &error);
  DCHECK(U_SUCCESS(error) && n > 0);

  // Checking Arabic and Hebrew scripts cover Arabic, Hebrew, Farsi,
  // Urdu and Azerbaijani written in Arabic. Syriac script
  // (another RTL) is not a living script and we didn't yet localize
  // to locales using other living RTL scripts such as Thaana and N'ko.
  // TODO(jungshik): Use a new ICU API, uloc_getCharacterOrientation to avoid
  // 'hardcoded-comparision' with Arabic and Hebrew scripts once we
  // upgrade ICU to 4.0 or later or port it to our copy of ICU.
  if (scripts[0] == USCRIPT_ARABIC || scripts[0] == USCRIPT_HEBREW)
    return RIGHT_TO_LEFT;
  return LEFT_TO_RIGHT;
}

TextDirection GetFirstStrongCharacterDirection(const std::wstring& text) {
#if defined(WCHAR_T_IS_UTF32)
  string16 text_utf16 = WideToUTF16(text);
  const UChar* string = text_utf16.c_str();
#else
  const UChar* string = text.c_str();
#endif
  size_t length = text.length();
  size_t position = 0;
  while (position < length) {
    UChar32 character;
    size_t next_position = position;
    U16_NEXT(string, next_position, length, character);

    // Now that we have the character, we use ICU in order to query for the
    // appropriate Unicode BiDi character type.
    int32_t property = u_getIntPropertyValue(character, UCHAR_BIDI_CLASS);
    if ((property == U_RIGHT_TO_LEFT) ||
        (property == U_RIGHT_TO_LEFT_ARABIC) ||
        (property == U_RIGHT_TO_LEFT_EMBEDDING) ||
        (property == U_RIGHT_TO_LEFT_OVERRIDE)) {
      return RIGHT_TO_LEFT;
    } else if ((property == U_LEFT_TO_RIGHT) ||
               (property == U_LEFT_TO_RIGHT_EMBEDDING) ||
               (property == U_LEFT_TO_RIGHT_OVERRIDE)) {
      return LEFT_TO_RIGHT;
    }

    position = next_position;
  }

  return LEFT_TO_RIGHT;
}

bool AdjustStringForLocaleDirection(const std::wstring& text,
                                    std::wstring* localized_text) {
  if (GetTextDirection() == LEFT_TO_RIGHT || text.length() == 0)
    return false;

  // Marking the string as LTR if the locale is RTL and the string does not
  // contain strong RTL characters. Otherwise, mark the string as RTL.
  *localized_text = text;
  bool has_rtl_chars = StringContainsStrongRTLChars(text);
  if (!has_rtl_chars)
    WrapStringWithLTRFormatting(localized_text);
  else
    WrapStringWithRTLFormatting(localized_text);

  return true;
}

bool StringContainsStrongRTLChars(const std::wstring& text) {
#if defined(WCHAR_T_IS_UTF32)
  string16 text_utf16 = WideToUTF16(text);
  const UChar* string = text_utf16.c_str();
#else
  const UChar* string = text.c_str();
#endif
  size_t length = text.length();
  size_t position = 0;
  while (position < length) {
    UChar32 character;
    size_t next_position = position;
    U16_NEXT(string, next_position, length, character);

    // Now that we have the character, we use ICU in order to query for the
    // appropriate Unicode BiDi character type.
    int32_t property = u_getIntPropertyValue(character, UCHAR_BIDI_CLASS);
    if ((property == U_RIGHT_TO_LEFT) || (property == U_RIGHT_TO_LEFT_ARABIC))
      return true;

    position = next_position;
  }

  return false;
}

void WrapStringWithLTRFormatting(std::wstring* text) {
  // Inserting an LRE (Left-To-Right Embedding) mark as the first character.
  text->insert(0, 1, static_cast<wchar_t>(kLeftToRightEmbeddingMark));

  // Inserting a PDF (Pop Directional Formatting) mark as the last character.
  text->push_back(static_cast<wchar_t>(kPopDirectionalFormatting));
}

void WrapStringWithRTLFormatting(std::wstring* text) {
  // Inserting an RLE (Right-To-Left Embedding) mark as the first character.
  text->insert(0, 1, static_cast<wchar_t>(kRightToLeftEmbeddingMark));

  // Inserting a PDF (Pop Directional Formatting) mark as the last character.
  text->push_back(static_cast<wchar_t>(kPopDirectionalFormatting));
}

void WrapPathWithLTRFormatting(const FilePath& path,
                               string16* rtl_safe_path) {
  // Wrap the overall path with LRE-PDF pair which essentialy marks the
  // string as a Left-To-Right string.
  // Inserting an LRE (Left-To-Right Embedding) mark as the first character.
  rtl_safe_path->push_back(kLeftToRightEmbeddingMark);
#if defined(OS_MACOSX)
    rtl_safe_path->append(UTF8ToUTF16(path.value()));
#elif defined(OS_WIN)
    rtl_safe_path->append(path.value());
#else  // defined(OS_LINUX)
    std::wstring wide_path = base::SysNativeMBToWide(path.value());
    rtl_safe_path->append(WideToUTF16(wide_path));
#endif
  // Inserting a PDF (Pop Directional Formatting) mark as the last character.
  rtl_safe_path->push_back(kPopDirectionalFormatting);
}

int DefaultCanvasTextAlignment() {
  if (GetTextDirection() == LEFT_TO_RIGHT) {
    return ChromeCanvas::TEXT_ALIGN_LEFT;
  } else {
    return ChromeCanvas::TEXT_ALIGN_RIGHT;
  }
}


// Compares the character data stored in two different strings by specified
// Collator instance.
UCollationResult CompareStringWithCollator(const Collator* collator,
                                           const std::wstring& lhs,
                                           const std::wstring& rhs) {
  DCHECK(collator);
  UErrorCode error = U_ZERO_ERROR;
#if defined(WCHAR_T_IS_UTF32)
  // Need to convert to UTF-16 to be compatible with UnicodeString's
  // constructor.
  string16 lhs_utf16 = WideToUTF16(lhs);
  string16 rhs_utf16 = WideToUTF16(rhs);

  UCollationResult result = collator->compare(
      static_cast<const UChar*>(lhs_utf16.c_str()),
      static_cast<int>(lhs_utf16.length()),
      static_cast<const UChar*>(rhs_utf16.c_str()),
      static_cast<int>(rhs_utf16.length()),
      error);
#else
  UCollationResult result = collator->compare(
      static_cast<const UChar*>(lhs.c_str()), static_cast<int>(lhs.length()),
      static_cast<const UChar*>(rhs.c_str()), static_cast<int>(rhs.length()),
      error);
#endif
  DCHECK(U_SUCCESS(error));
  return result;
}

// Specialization of operator() method for std::wstring version.
template <>
bool StringComparator<std::wstring>::operator()(const std::wstring& lhs,
                                                const std::wstring& rhs) {
  // If we can not get collator instance for specified locale, just do simple
  // string compare.
  if (!collator_)
    return lhs < rhs;
  return CompareStringWithCollator(collator_, lhs, rhs) == UCOL_LESS;
};

void SortStrings(const std::wstring& locale,
                 std::vector<std::wstring>* strings) {
  SortVectorWithStringKey(locale, strings, false);
}

const std::vector<std::string>& GetAvailableLocales() {
  static std::vector<std::string> locales;
  if (locales.empty()) {
    int num_locales = uloc_countAvailable();
    for (int i = 0; i < num_locales; ++i) {
      std::string locale_name = uloc_getAvailable(i);
      // Filter out the names that have aliases.
      if (IsDuplicateName(locale_name))
        continue;
      if (!IsLocaleSupportedByOS(ASCIIToWide(locale_name)))
        continue;
      // Normalize underscores to hyphens because that's what our locale files
      // use.
      std::replace(locale_name.begin(), locale_name.end(), '_', '-');

      // Map the Chinese locale names over to zh-CN and zh-TW.
      if (LowerCaseEqualsASCII(locale_name, "zh-hans")) {
        locale_name = "zh-CN";
      } else if (LowerCaseEqualsASCII(locale_name, "zh-hant")) {
        locale_name = "zh-TW";
      }
      locales.push_back(locale_name);
    }

    // Manually add 'es-419' to the list. See the comment in IsDuplicateName().
    locales.push_back("es-419");
  }
  return locales;
}

BiDiLineIterator::~BiDiLineIterator() {
  if (bidi_) {
    ubidi_close(bidi_);
    bidi_ = NULL;
  }
}

UBool BiDiLineIterator::Open(const std::wstring& text,
                             bool right_to_left,
                             bool url) {
  DCHECK(bidi_ == NULL);
  UErrorCode error = U_ZERO_ERROR;
  bidi_ = ubidi_openSized(static_cast<int>(text.length()), 0, &error);
  if (U_FAILURE(error))
    return false;
  if (right_to_left && url)
    ubidi_setReorderingMode(bidi_, UBIDI_REORDER_RUNS_ONLY);
#if defined(WCHAR_T_IS_UTF32)
  const string16 text_utf16 = WideToUTF16(text);
#else
  const std::wstring &text_utf16 = text;
#endif  // U_SIZEOF_WCHAR_T != 4
  ubidi_setPara(bidi_, text_utf16.data(), static_cast<int>(text_utf16.length()),
                right_to_left ? UBIDI_DEFAULT_RTL : UBIDI_DEFAULT_LTR,
                NULL, &error);
  return U_SUCCESS(error);
}

int BiDiLineIterator::CountRuns() {
  DCHECK(bidi_ != NULL);
  UErrorCode error = U_ZERO_ERROR;
  const int runs = ubidi_countRuns(bidi_, &error);
  return U_SUCCESS(error) ? runs : 0;
}

UBiDiDirection BiDiLineIterator::GetVisualRun(int index,
                                              int* start,
                                              int* length) {
  DCHECK(bidi_ != NULL);
  return ubidi_getVisualRun(bidi_, index, start, length);
}

void BiDiLineIterator::GetLogicalRun(int start,
                                     int* end,
                                     UBiDiLevel* level) {
  DCHECK(bidi_ != NULL);
  ubidi_getLogicalRun(bidi_, start, end, level);
}

}
