// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"

#include <algorithm>

#include "chrome/common/l10n_util.h"

#include "base/command_line.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/scoped_ptr.h"
#include "base/string_util.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/gfx/chrome_canvas.h"
#if defined(OS_WIN)
// TODO(port): re-enable.
#include "chrome/common/resource_bundle.h"
#include "chrome/views/view.h"
#endif  // defined(OS_WIN)
#include "unicode/coll.h"
#include "unicode/locid.h"
#include "unicode/rbbi.h"
#include "unicode/uchar.h"

// TODO(playmobil): remove this undef once SkPostConfig.h is fixed.
// skia/include/corecg/SkPostConfig.h #defines strcasecmp() so we can't use
// base::strcasecmp() without #undefing it here.
#undef strcasecmp

namespace {

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

// Convert Chrome locale name (DLL name) to ICU locale name
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

// Compares two wstrings and returns true if the first arg is less than the
// second arg.  This uses the locale specified in the constructor.
class StringComparator : public std::binary_function<const std::wstring&,
                                                     const std::wstring&,
                                                     bool> {
 public:
  explicit StringComparator(Collator* collator)
      : collator_(collator) { }

  // Returns true if lhs preceeds rhs.
  bool operator() (const std::wstring& lhs, const std::wstring& rhs) {
    UErrorCode error = U_ZERO_ERROR;
#if defined(WCHAR_T_IS_UTF32)
    // Need to convert to UTF-16 to be compatible with UnicodeString's
    // constructor.
    string16 lhs_utf16 = WideToUTF16(lhs);
    string16 rhs_utf16 = WideToUTF16(rhs);

    UCollationResult result = collator_->compare(
        static_cast<const UChar*>(lhs_utf16.c_str()),
        static_cast<int>(lhs_utf16.length()),
        static_cast<const UChar*>(rhs_utf16.c_str()),
        static_cast<int>(rhs_utf16.length()),
        error);
#else
    UCollationResult result = collator_->compare(
        static_cast<const UChar*>(lhs.c_str()), static_cast<int>(lhs.length()),
        static_cast<const UChar*>(rhs.c_str()), static_cast<int>(rhs.length()),
        error);
#endif
    DCHECK(U_SUCCESS(error));

    return result == UCOL_LESS;
  }

 private:
  Collator* collator_;
};

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
  // load it because it may be pointing outside the locale dll directory.
  file_util::ReplaceIllegalCharacters(&test_locale, ' ');
  if (test_locale != locale)
    return false;

  std::wstring test_path = locale_path;
  file_util::AppendToPath(&test_path, locale + L".dll");
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
  std::wstring locale_path;
  PathService::Get(chrome::DIR_LOCALES, &locale_path);
  std::wstring resolved_locale;

  // First, check to see if there's a --lang flag.
  CommandLine parsed_command_line;
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

  // No DLL, we shouldn't get here.
  NOTREACHED();
  return std::wstring();
}

std::wstring GetLocalName(const std::wstring& locale_code_wstr,
                          const std::wstring& app_locale_wstr,
                          bool is_for_ui) {
  std::string locale_code_str = WideToASCII(locale_code_wstr);
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
  if (is_for_ui && GetTextDirection() == RIGHT_TO_LEFT)
    return name_local + kRightToLeftMark;
  else
    return name_local;
}

std::wstring GetString(int message_id) {
#if defined(OS_WIN)
  ResourceBundle &rb = ResourceBundle::GetSharedInstance();
  return rb.GetLocalizedString(message_id);
#else
  NOTIMPLEMENTED();  // TODO(port): Real implementation of GetString.
  return L"";
#endif
}

#if defined(OS_WIN)
// TODO(port): re-enable.
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
#endif  // defined(OS_WIN)

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
      RuleBasedBreakIterator::createLineInstance(Locale::getDefault(), status)));
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

// Returns the text direction.
// This function retrieves the language corresponding to the default ICU locale
// (assuming that SetICUDefaultLocale is called) and determines the text
// direction by comparing it with "ar" or "he".
// Note that script is better than language here to get a wider coverage.
// Unfortunately, getScript in ICU returns an empty string unless
// the locale is created with an explicit script specified. For now,
// it does not matter much because we only support Hebrew and Arabic.
// (c.f. other languages written in RTL : Farsi, Urdu, Syriac, Azerbaijani
//  in Arabic, etc)
// TODO(hbono): Need to find better identification methods than checking
// if the language ID is Arabic or Hebrew. (http://b/issue?id=1054119)
// Use an ICU API when added (see http://bugs.icu-project.org/trac/ticket/6228).
TextDirection GetTextDirection() {
  if (g_text_direction == UNKNOWN_DIRECTION) {
    const Locale& locale = Locale::getDefault();
    const char* lang = locale.getLanguage();
    // Check only for Arabic and Hebrew languages now.
    if (strcmp(lang, "ar") == 0 || strcmp(lang, "he") == 0) {
      g_text_direction = RIGHT_TO_LEFT;
    } else {
      g_text_direction = LEFT_TO_RIGHT;
    }
  }
  return g_text_direction;
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
  const wchar_t* string = text.c_str();
  int length = static_cast<int>(text.length());
  int position = 0;
  while (position < length) {
    UChar32 character;
    int next_position = position;
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
  text->insert(0, L"\x202A");

  // Inserting a PDF (Pop Directional Formatting) mark as the last character.
  text->append(L"\x202C");
}

void WrapStringWithRTLFormatting(std::wstring* text) {
  // Inserting an RLE (Right-To-Left Embedding) mark as the first character.
  text->insert(0, L"\x202B");

  // Inserting a PDF (Pop Directional Formatting) mark as the last character.
  text->append(L"\x202C");
}

int DefaultCanvasTextAlignment() {
  if (GetTextDirection() == LEFT_TO_RIGHT) {
    return ChromeCanvas::TEXT_ALIGN_LEFT;
  } else {
    return ChromeCanvas::TEXT_ALIGN_RIGHT;
  }
}

#if defined(OS_WIN)
int GetExtendedStyles() {
  return GetTextDirection() == LEFT_TO_RIGHT ? 0 :
      WS_EX_LAYOUTRTL | WS_EX_RTLREADING;
}

int GetExtendedTooltipStyles() {
  return GetTextDirection() == LEFT_TO_RIGHT ? 0 : WS_EX_LAYOUTRTL;
}

void HWNDSetRTLLayout(HWND hwnd) {
  DWORD ex_style = ::GetWindowLong(hwnd, GWL_EXSTYLE);

  // We don't have to do anything if the style is already set for the HWND.
  if (!(ex_style & WS_EX_LAYOUTRTL)) {
    ex_style |= WS_EX_LAYOUTRTL;
    ::SetWindowLong(hwnd, GWL_EXSTYLE, ex_style);

    // Right-to-left layout changes are not applied to the window immediately
    // so we should make sure a WM_PAINT is sent to the window by invalidating
    // the entire window rect.
    ::InvalidateRect(hwnd, NULL, true);
  }
}
#endif  // defined(OS_WIN)

void SortStrings(const std::wstring& locale,
                 std::vector<std::wstring>* strings) {
  UErrorCode error = U_ZERO_ERROR;
  Locale loc(WideToUTF8(locale).c_str());
  scoped_ptr<Collator> collator(Collator::createInstance(loc, error));
  if (U_FAILURE(error)) {
    // Just do an string sort.
    sort(strings->begin(), strings->end());
    return;
  }
  StringComparator c(collator.get());
  sort(strings->begin(), strings->end(), c);
}

const std::vector<std::wstring>& GetAvailableLocales() {
  static std::vector<std::wstring> locales;
  if (locales.empty()) {
    int num_locales = uloc_countAvailable();
    for (int i = 0; i < num_locales; ++i) {
      std::string locale_name = uloc_getAvailable(i);
      // Filter out the names that have aliases.
      if (IsDuplicateName(locale_name))
        continue;
      // Normalize underscores to hyphens because that's what our locale dlls
      // use.
      std::replace(locale_name.begin(), locale_name.end(), '_', '-');

      // Map the Chinese locale names over to zh-CN and zh-TW.
      if (LowerCaseEqualsASCII(locale_name, "zh-hans")) {
        locale_name = "zh-CN";
      } else if (LowerCaseEqualsASCII(locale_name, "zh-hant")) {
        locale_name = "zh-TW";
      }
      locales.push_back(ASCIIToWide(locale_name));
    }

    // Manually add 'es-419' to the list. See the comment in IsDuplicateName().
    locales.push_back(L"es-419");
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
