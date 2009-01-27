#include <atlbase.h>
#include <shlwapi.h>

#include <map>

#include "base/logging.h"
#include "base/string_util.h"

#include "installer_util_strings.h"

namespace {

// Gets the language from the OS.  If we're unable to get the system language,
// defaults to en-us.
std::wstring GetSystemLanguage() {
  static std::wstring language;
  if (!language.empty())
    return language;
  // We don't have ICU at this point, so we use win32 apis.
  LCID id = GetThreadLocale();
  int length = GetLocaleInfo(id, LOCALE_SISO639LANGNAME, 0, 0);
  if (0 == length) {
    language = L"en-us";
    return language;
  }
  length = GetLocaleInfo(id, LOCALE_SISO639LANGNAME,
                         WriteInto(&language, length), length);
  DCHECK(length == language.length() + 1);
  StringToLowerASCII(&language);

  // Add the country if we need it.
  std::wstring country;
  length = GetLocaleInfo(id, LOCALE_SISO3166CTRYNAME, 0, 0);
  if (0 != length) {
    length = GetLocaleInfo(id, LOCALE_SISO3166CTRYNAME,
                           WriteInto(&country, length), length);
    DCHECK(length == country.length() + 1);
    StringToLowerASCII(&country);
    if (L"en" == language) {
      if (L"gb" == country) {
        language.append(L"-gb");
      } else {
        language.append(L"-us");
      }
    } else if (L"es" == language && L"es" != country) {
      language.append(L"-419");
    } else if (L"pt" == language) {
      if (L"br" == country) {
        language.append(L"-br");
      } else {
        language.append(L"-pt");
      }
    } else if (L"zh" == language) {
      if (L"tw" == country || L"mk" == country || L"hk" == country) {
        language.append(L"-tw");
      } else {
        language.append(L"-cn");
      }
    }
  }

  if (language.empty())
    language = L"en-us";

  return language;
}

// This method returns the appropriate language offset given the language as a
// string.  Note: This method is not thread safe because of how we create
// |offset_map|.
int GetLanguageOffset(const std::wstring& language) {
  static std::map<std::wstring, int> offset_map;
  if (offset_map.empty()) {
    offset_map[L"ar"] = IDS_L10N_OFFSET_AR;
    offset_map[L"bg"] = IDS_L10N_OFFSET_BG;
    offset_map[L"ca"] = IDS_L10N_OFFSET_CA;
    offset_map[L"cs"] = IDS_L10N_OFFSET_CS;
    offset_map[L"da"] = IDS_L10N_OFFSET_DA;
    offset_map[L"de"] = IDS_L10N_OFFSET_DE;
    offset_map[L"el"] = IDS_L10N_OFFSET_EL;
    offset_map[L"en-gb"] = IDS_L10N_OFFSET_EN_GB;
    offset_map[L"en-us"] = IDS_L10N_OFFSET_EN_US;
    offset_map[L"es"] = IDS_L10N_OFFSET_ES;
    offset_map[L"es-419"] = IDS_L10N_OFFSET_ES_419;
    offset_map[L"et"] = IDS_L10N_OFFSET_ET;
    offset_map[L"fi"] = IDS_L10N_OFFSET_FI;
    offset_map[L"fil"] = IDS_L10N_OFFSET_FIL;
    offset_map[L"fr"] = IDS_L10N_OFFSET_FR;
    offset_map[L"he"] = IDS_L10N_OFFSET_HE;
    offset_map[L"hi"] = IDS_L10N_OFFSET_HI;
    offset_map[L"hr"] = IDS_L10N_OFFSET_HR;
    offset_map[L"hu"] = IDS_L10N_OFFSET_HU;
    offset_map[L"id"] = IDS_L10N_OFFSET_ID;
    offset_map[L"it"] = IDS_L10N_OFFSET_IT;
    // Google web properties use iw for he. Handle both just to be safe.
    offset_map[L"iw"] = IDS_L10N_OFFSET_HE;
    offset_map[L"ja"] = IDS_L10N_OFFSET_JA;
    offset_map[L"ko"] = IDS_L10N_OFFSET_KO;
    offset_map[L"lt"] = IDS_L10N_OFFSET_LT;
    offset_map[L"lv"] = IDS_L10N_OFFSET_LV;
    // Google web properties use no for nb. Handle both just to be safe.
    offset_map[L"nb"] = IDS_L10N_OFFSET_NO;
    offset_map[L"nl"] = IDS_L10N_OFFSET_NL;
    offset_map[L"no"] = IDS_L10N_OFFSET_NO;
    offset_map[L"pl"] = IDS_L10N_OFFSET_PL;
    offset_map[L"pt-br"] = IDS_L10N_OFFSET_PT_BR;
    offset_map[L"pt-pt"] = IDS_L10N_OFFSET_PT_PT;
    offset_map[L"ro"] = IDS_L10N_OFFSET_RO;
    offset_map[L"ru"] = IDS_L10N_OFFSET_RU;
    offset_map[L"sk"] = IDS_L10N_OFFSET_SK;
    offset_map[L"sl"] = IDS_L10N_OFFSET_SL;
    offset_map[L"sr"] = IDS_L10N_OFFSET_SR;
    offset_map[L"sv"] = IDS_L10N_OFFSET_SV;
    offset_map[L"th"] = IDS_L10N_OFFSET_TH;
    // Some Google web properties use tl for fil. Handle both just to be safe.
    // They're not completely identical, but alias it here.
    offset_map[L"tl"] = IDS_L10N_OFFSET_FIL;
    offset_map[L"tr"] = IDS_L10N_OFFSET_TR;
    offset_map[L"uk"] = IDS_L10N_OFFSET_UK;
    offset_map[L"vi"] = IDS_L10N_OFFSET_VI;
    offset_map[L"zh-cn"] = IDS_L10N_OFFSET_ZH_CN;
    offset_map[L"zh-tw"] = IDS_L10N_OFFSET_ZH_TW;
  }

  std::map<std::wstring, int>::iterator it = offset_map.find(
      StringToLowerASCII(language));
  if (it != offset_map.end())
    return it->second;

  NOTREACHED() << "unknown system language-country";

  // Fallback on the en-US offset just in case.
  return IDS_L10N_OFFSET_EN_US;
}

}  // namespace

namespace installer_util {

std::wstring GetLocalizedString(int base_message_id) {
  std::wstring language = GetSystemLanguage();
  std::wstring localized_string;

  int message_id = base_message_id + GetLanguageOffset(language);
  const ATLSTRINGRESOURCEIMAGE* image = AtlGetStringResourceImage(
      _AtlBaseModule.GetModuleInstance(), message_id);
  if (image) {
    localized_string = std::wstring(image->achString, image->nLength);
  } else {
    NOTREACHED() << "Unable to find resource id " << message_id;
  }

  return localized_string;
}

// Here we generate the url spec with the Microsoft res:// scheme which is
// explained here : http://support.microsoft.com/kb/220830
std::wstring GetLocalizedEulaResource() {
  wchar_t full_exe_path[MAX_PATH];
  int len = ::GetModuleFileNameW(NULL, full_exe_path, MAX_PATH);
  if (len <= 0 && len >= MAX_PATH)
    return L"";
  // The default language is English, but we also support Spanish,French and
  // Portuguese.
  std::wstring language = GetSystemLanguage();
  const wchar_t* resource = L"IDR_OEMPG_EN.HTML";
  if (language == L"fr")
    resource = L"IDR_OEMPG_FR.HTML";
  else if (language == L"es")
    resource = L"IDR_OEMPG_ES.HTML";
  else if (language == L"pt-br")
    resource = L"IDR_OEMPG_BR.HTML";
  // Spaces and DOS paths must be url encoded.
  std::wstring url_path =
      StringPrintf(L"res://%ls/#23/%ls", full_exe_path, resource);
  DWORD count = url_path.size() * 3;
  scoped_ptr<wchar_t> url_canon(new wchar_t[count]);
  HRESULT hr = ::UrlCanonicalizeW(url_path.c_str(), url_canon.get(),
                                  &count, URL_ESCAPE_UNSAFE);
  if (SUCCEEDED(hr))
    return std::wstring(url_canon.get());
  return url_path;
}

}  // namespace installer_util

