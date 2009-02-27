// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/character_encoding.h"

#include <map>
#include <set>

#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "base/string_tokenizer.h"
#include "base/string_util.h"
#include "chrome/app/chrome_dll_resource.h"
#include "chrome/common/l10n_util.h"
#include "grit/generated_resources.h"
#include "unicode/ucnv.h"

namespace {

// The maximum length of short list of recently user selected encodings is 3.
const size_t kUserSelectedEncodingsMaxLength = 3;

typedef struct {
  int resource_id;
  const wchar_t* name;
  int category_string_id;
} CanonicalEncodingData;

// An array of all supported canonical encoding names.
static CanonicalEncodingData canonical_encoding_names[] = {
  { IDC_ENCODING_UTF8, L"UTF-8", IDS_ENCODING_UNICODE },
  { IDC_ENCODING_UTF16LE, L"UTF-16LE", IDS_ENCODING_UNICODE },
  { IDC_ENCODING_ISO88591, L"ISO-8859-1", IDS_ENCODING_WESTERN },
  { IDC_ENCODING_WINDOWS1252, L"windows-1252", IDS_ENCODING_WESTERN },
  { IDC_ENCODING_GBK, L"GBK", IDS_ENCODING_SIMP_CHINESE },
  { IDC_ENCODING_GB18030, L"gb18030", IDS_ENCODING_SIMP_CHINESE },
  { IDC_ENCODING_BIG5, L"Big5", IDS_ENCODING_TRAD_CHINESE },
  { IDC_ENCODING_BIG5HKSCS, L"Big5-HKSCS", IDS_ENCODING_TRAD_CHINESE },
  { IDC_ENCODING_KOREAN, L"windows-949", IDS_ENCODING_KOREAN },
  { IDC_ENCODING_SHIFTJIS, L"Shift_JIS", IDS_ENCODING_JAPANESE },
  { IDC_ENCODING_EUCJP, L"EUC-JP", IDS_ENCODING_JAPANESE },
  { IDC_ENCODING_ISO2022JP, L"ISO-2022-JP", IDS_ENCODING_JAPANESE },
  { IDC_ENCODING_THAI, L"windows-874", IDS_ENCODING_THAI },
  { IDC_ENCODING_ISO885915, L"ISO-8859-15", IDS_ENCODING_WESTERN },
  { IDC_ENCODING_MACINTOSH, L"macintosh", IDS_ENCODING_WESTERN },
  { IDC_ENCODING_ISO88592, L"ISO-8859-2", IDS_ENCODING_CENTRAL_EUROPEAN },
  { IDC_ENCODING_WINDOWS1250, L"windows-1250", IDS_ENCODING_CENTRAL_EUROPEAN },
  { IDC_ENCODING_ISO88595, L"ISO-8859-5", IDS_ENCODING_CYRILLIC },
  { IDC_ENCODING_WINDOWS1251, L"windows-1251", IDS_ENCODING_CYRILLIC },
  { IDC_ENCODING_KOI8R, L"KOI8-R", IDS_ENCODING_CYRILLIC },
  { IDC_ENCODING_KOI8U, L"KOI8-U", IDS_ENCODING_CYRILLIC },
  { IDC_ENCODING_ISO88597, L"ISO-8859-7", IDS_ENCODING_GREEK },
  { IDC_ENCODING_WINDOWS1253, L"windows-1253", IDS_ENCODING_GREEK },
  { IDC_ENCODING_WINDOWS1254, L"windows-1254", IDS_ENCODING_TURKISH },
  { IDC_ENCODING_ISO88596, L"ISO-8859-6", IDS_ENCODING_ARABIC },
  { IDC_ENCODING_WINDOWS1256, L"windows-1256", IDS_ENCODING_ARABIC },
  { IDC_ENCODING_ISO88598, L"ISO-8859-8", IDS_ENCODING_HEBREW },
  { IDC_ENCODING_WINDOWS1255, L"windows-1255", IDS_ENCODING_HEBREW },
  { IDC_ENCODING_WINDOWS1258, L"windows-1258", IDS_ENCODING_VIETNAMESE },
  { IDC_ENCODING_ISO88594, L"ISO-8859-4", IDS_ENCODING_BALTIC },
  { IDC_ENCODING_ISO885913, L"ISO-8859-13", IDS_ENCODING_BALTIC },
  { IDC_ENCODING_WINDOWS1257, L"windows-1257", IDS_ENCODING_BALTIC },
  { IDC_ENCODING_ISO88593, L"ISO-8859-3", IDS_ENCODING_SOUTH_EUROPEAN },
  { IDC_ENCODING_ISO885910, L"ISO-8859-10", IDS_ENCODING_NORDIC },
  { IDC_ENCODING_ISO885914, L"ISO-8859-14", IDS_ENCODING_CELTIC },
  { IDC_ENCODING_ISO885916, L"ISO-8859-16", IDS_ENCODING_ROMANIAN },
};

static const int canonical_encoding_names_length =
    arraysize(canonical_encoding_names);

typedef std::map<int, std::pair<const wchar_t*, int> >
    IdToCanonicalEncodingNameMapType;
typedef std::map<const std::wstring, int> CanonicalEncodingNameToIdMapType;

class CanonicalEncodingMap {
 public:
  CanonicalEncodingMap()
      : id_to_encoding_name_map_(NULL),
        encoding_name_to_id_map_(NULL) { }
  const IdToCanonicalEncodingNameMapType* GetIdToCanonicalEncodingNameMapData();
  const CanonicalEncodingNameToIdMapType* GetCanonicalEncodingNameToIdMapData();
  std::vector<int>* const locale_dependent_encoding_ids() {
    return &locale_dependent_encoding_ids_;
  }

  std::vector<CharacterEncoding::EncodingInfo>* const current_display_encodings() {
    return &current_display_encodings_;
  }

 private:
  scoped_ptr<IdToCanonicalEncodingNameMapType> id_to_encoding_name_map_;
  scoped_ptr<CanonicalEncodingNameToIdMapType> encoding_name_to_id_map_;
  std::vector<int> locale_dependent_encoding_ids_;
  std::vector<CharacterEncoding::EncodingInfo> current_display_encodings_;

  DISALLOW_EVIL_CONSTRUCTORS(CanonicalEncodingMap);
};

const IdToCanonicalEncodingNameMapType* CanonicalEncodingMap::GetIdToCanonicalEncodingNameMapData() {
  // Testing and building map is not thread safe, this function is supposed to
  // only run in UI thread. Myabe I should add a lock in here for making it as
  // thread safe.
  if (!id_to_encoding_name_map_.get()) {
    id_to_encoding_name_map_.reset(new IdToCanonicalEncodingNameMapType);
    for (int i = 0; i < canonical_encoding_names_length; ++i) {
      int resource_id = canonical_encoding_names[i].resource_id;
      (*id_to_encoding_name_map_)[resource_id] =
        std::make_pair(canonical_encoding_names[i].name,
                       canonical_encoding_names[i].category_string_id);
    }
  }
  return id_to_encoding_name_map_.get();
}

const CanonicalEncodingNameToIdMapType* CanonicalEncodingMap::GetCanonicalEncodingNameToIdMapData() {
  if (!encoding_name_to_id_map_.get()) {
    encoding_name_to_id_map_.reset(new CanonicalEncodingNameToIdMapType);
    for (int i = 0; i < canonical_encoding_names_length; ++i) {
      (*encoding_name_to_id_map_)[canonical_encoding_names[i].name] =
          canonical_encoding_names[i].resource_id;
    }
  }
  return encoding_name_to_id_map_.get();
}

// A static map object which contains all resourceid-nonsequenced canonical
// encoding names.
static CanonicalEncodingMap canonical_encoding_name_map_singleton;

const int default_encoding_menus[] = {
  IDC_ENCODING_UTF16LE,
  IDC_ENCODING_ISO88591,
  IDC_ENCODING_WINDOWS1252,
  IDC_ENCODING_GBK,
  IDC_ENCODING_GB18030,
  IDC_ENCODING_BIG5,
  IDC_ENCODING_BIG5HKSCS,
  IDC_ENCODING_KOREAN,
  IDC_ENCODING_SHIFTJIS,
  IDC_ENCODING_EUCJP,
  IDC_ENCODING_ISO2022JP,
  IDC_ENCODING_THAI,
  IDC_ENCODING_ISO885915,
  IDC_ENCODING_MACINTOSH,
  IDC_ENCODING_ISO88592,
  IDC_ENCODING_WINDOWS1250,
  IDC_ENCODING_ISO88595,
  IDC_ENCODING_WINDOWS1251,
  IDC_ENCODING_KOI8R,
  IDC_ENCODING_KOI8U,
  IDC_ENCODING_ISO88597,
  IDC_ENCODING_WINDOWS1253,
  IDC_ENCODING_WINDOWS1254,
  IDC_ENCODING_ISO88596,
  IDC_ENCODING_WINDOWS1256,
  IDC_ENCODING_ISO88598,
  IDC_ENCODING_WINDOWS1255,
  IDC_ENCODING_WINDOWS1258,
  IDC_ENCODING_ISO88594,
  IDC_ENCODING_ISO885913,
  IDC_ENCODING_WINDOWS1257,
  IDC_ENCODING_ISO88593,
  IDC_ENCODING_ISO885910,
  IDC_ENCODING_ISO885914,
  IDC_ENCODING_ISO885916,
};

const int default_encoding_menus_length = arraysize(default_encoding_menus);

// Parse the input |encoding_list| which is a encoding list separated with
// comma, get available encoding ids and save them to |available_list|.
// The parameter |maximum_size| indicates maximum size of encoding items we
// want to get from the |encoding_list|.
void ParseEncodingListSeparatedWithComma(
    const std::wstring& encoding_list, std::vector<int>* const available_list,
    size_t maximum_size) {
  WStringTokenizer tokenizer(encoding_list, L",");
  while (tokenizer.GetNext()) {
    int id = CharacterEncoding::GetCommandIdByCanonicalEncodingName(
        tokenizer.token());
    // Ignore invalid encoding.
    if (!id)
      continue;
    available_list->push_back(id);
    if (available_list->size() == maximum_size)
      return;
  }
}

std::wstring GetEncodingDisplayName(std::wstring encoding_name,
                                    int category_string_id) {
  std::wstring category_name = l10n_util::GetString(category_string_id);
  if (category_string_id != IDS_ENCODING_KOREAN &&
      category_string_id != IDS_ENCODING_THAI &&
      category_string_id != IDS_ENCODING_TURKISH) {
    return l10n_util::GetStringF(IDS_ENCODING_DISPLAY_TEMPLATE,
                                 category_name,
                                 encoding_name);
  }
  return category_name;
}

int GetEncodingCategoryStringIdByCommandId(int id) {
  const IdToCanonicalEncodingNameMapType* map =
      canonical_encoding_name_map_singleton.
          GetIdToCanonicalEncodingNameMapData();
  DCHECK(map);

  IdToCanonicalEncodingNameMapType::const_iterator found_name = map->find(id);
  if (found_name != map->end())
    return found_name->second.second;
  return 0;
}

std::wstring GetEncodingCategoryStringByCommandId(int id) {
  int category_id = GetEncodingCategoryStringIdByCommandId(id);
  if (category_id)
    return l10n_util::GetString(category_id);
  return std::wstring();
}

}  // namespace

CharacterEncoding::EncodingInfo::EncodingInfo(int id)
    : encoding_id(id) {
  encoding_category_name = GetEncodingCategoryStringByCommandId(id),
  encoding_display_name = GetCanonicalEncodingDisplayNameByCommandId(id);
}

// Static.
int CharacterEncoding::GetCommandIdByCanonicalEncodingName(
    const std::wstring& encoding_name) {
  const CanonicalEncodingNameToIdMapType* map =
      canonical_encoding_name_map_singleton.
          GetCanonicalEncodingNameToIdMapData();
  DCHECK(map);

  CanonicalEncodingNameToIdMapType::const_iterator found_id =
      map->find(encoding_name);
  if (found_id != map->end())
    return found_id->second;
  return 0;
}

// Static.
std::wstring CharacterEncoding::GetCanonicalEncodingNameByCommandId(int id) {
  const IdToCanonicalEncodingNameMapType* map =
      canonical_encoding_name_map_singleton.
          GetIdToCanonicalEncodingNameMapData();
  DCHECK(map);

  IdToCanonicalEncodingNameMapType::const_iterator found_name = map->find(id);
  if (found_name != map->end())
    return found_name->second.first;
  return std::wstring();
}

// Static.
std::wstring CharacterEncoding::GetCanonicalEncodingDisplayNameByCommandId(
    int id) {
  const IdToCanonicalEncodingNameMapType* map =
      canonical_encoding_name_map_singleton.
          GetIdToCanonicalEncodingNameMapData();
  DCHECK(map);

  IdToCanonicalEncodingNameMapType::const_iterator found_name = map->find(id);
  if (found_name != map->end())
    return GetEncodingDisplayName(found_name->second.first,
                                  found_name->second.second);
  return std::wstring();
}

// Static.
// Return count number of all supported canonical encoding.
int CharacterEncoding::GetSupportCanonicalEncodingCount() {
  return canonical_encoding_names_length;
}

// Static.
std::wstring CharacterEncoding::GetCanonicalEncodingNameByIndex(int index) {
  if (index < canonical_encoding_names_length)
    return canonical_encoding_names[index].name;
  return std::wstring();
}

// Static.
std::wstring CharacterEncoding::GetCanonicalEncodingDisplayNameByIndex(
    int index) {
  if (index < canonical_encoding_names_length)
    return GetEncodingDisplayName(canonical_encoding_names[index].name,
        canonical_encoding_names[index].category_string_id);
  return std::wstring();
}

// Static.
int CharacterEncoding::GetEncodingCommandIdByIndex(int index) {
  if (index < canonical_encoding_names_length)
    return canonical_encoding_names[index].resource_id;
  return 0;
}

// Static.
std::wstring CharacterEncoding::GetCanonicalEncodingNameByAliasName(
    const std::wstring& alias_name) {
  // If the input alias_name is already canonical encoding name, just return it.
  const CanonicalEncodingNameToIdMapType* map =
      canonical_encoding_name_map_singleton.
          GetCanonicalEncodingNameToIdMapData();
  DCHECK(map);

  CanonicalEncodingNameToIdMapType::const_iterator found_id =
      map->find(alias_name);
  if (found_id != map->end())
    return alias_name;

  UErrorCode error_code = U_ZERO_ERROR;

  const char* canonical_name = ucnv_getCanonicalName(
      WideToASCII(alias_name).c_str(), "MIME", &error_code);
  // If failed,  then try IANA next.
  if (U_FAILURE(error_code) || !canonical_name) {
    error_code = U_ZERO_ERROR;
    canonical_name = ucnv_getCanonicalName(
      WideToASCII(alias_name).c_str(), "IANA", &error_code);
  }

  if (canonical_name)
    return ASCIIToWide(canonical_name);
  else
    return std::wstring();
}

// Static
// According to the behavior of user recently selected encoding short list in
// FireFox, we always put UTF-8 as toppest position, after then put user
// recently selected encodings, then put local dependent encoding items.
// At last, we put all rest encoding items.
const std::vector<CharacterEncoding::EncodingInfo>* CharacterEncoding::GetCurrentDisplayEncodings(
    const std::wstring& locale,
    const std::wstring& locale_encodings,
    const std::wstring& recently_select_encodings) {
  std::vector<int>* const locale_dependent_encoding_list =
      canonical_encoding_name_map_singleton.locale_dependent_encoding_ids();
  std::vector<CharacterEncoding::EncodingInfo>* const encoding_list =
      canonical_encoding_name_map_singleton.current_display_encodings();

  // Initialize locale dependent static encoding list.
  if (locale_dependent_encoding_list->empty() && !locale_encodings.empty())
    ParseEncodingListSeparatedWithComma(locale_encodings,
                                        locale_dependent_encoding_list,
                                        kUserSelectedEncodingsMaxLength);

  static std::wstring cached_user_selected_encodings;
  // Build current display encoding list.
  if (encoding_list->empty() ||
      cached_user_selected_encodings != recently_select_encodings) {
    // Update user recently selected encodings.
    cached_user_selected_encodings = recently_select_encodings;
    // Clear old encoding list since user recently selected encodings changed.
    encoding_list->clear();
    // Always add UTF-8 to first encoding position.
    encoding_list->push_back(EncodingInfo(IDC_ENCODING_UTF8));
    std::set<int> inserted_encoding;
    inserted_encoding.insert(IDC_ENCODING_UTF8);

    // Parse user recently selected encodings and get list
    std::vector<int> recently_select_encoding_list;
    ParseEncodingListSeparatedWithComma(recently_select_encodings,
                                        &recently_select_encoding_list,
                                        kUserSelectedEncodingsMaxLength);

    // Put 'cached encodings' (dynamic encoding list) after 'local dependent
    // encoding list'.
    recently_select_encoding_list.insert(recently_select_encoding_list.begin(),
        locale_dependent_encoding_list->begin(),
        locale_dependent_encoding_list->end());
    for (std::vector<int>::iterator it = recently_select_encoding_list.begin();
         it != recently_select_encoding_list.end(); ++it) {
        // Test whether we have met this encoding id.
        bool ok = inserted_encoding.insert(*it).second;
        // Duplicated encoding, ignore it. Ideally, this situation should not
        // happened, but just in case some one manually edit preference file.
        if (!ok)
          continue;
        encoding_list->push_back(EncodingInfo(*it));
    }
    // Append a separator;
    encoding_list->push_back(EncodingInfo(0));

    // We need to keep "Unicode (UTF-16LE)" always at the top (among the rest
    // of encodings) instead of being sorted along with other encodings. So if
    // "Unicode (UTF-16LE)" is already in previous encodings, sort the rest
    // of encodings. Otherwise Put "Unicode (UTF-16LE)" on the first of the
    // rest of encodings, skip "Unicode (UTF-16LE)" and sort all left encodings.
    int start_sorted_index = encoding_list->size();
    if (inserted_encoding.find(IDC_ENCODING_UTF16LE) ==
        inserted_encoding.end()) {
      encoding_list->push_back(EncodingInfo(IDC_ENCODING_UTF16LE));
      inserted_encoding.insert(IDC_ENCODING_UTF16LE);
      start_sorted_index++;
    }

    // Add the rest of encodings that are neither in the static encoding list
    // nor in the list of recently selected encodings.
    // Build the encoding list sorted in the current locale sorting order.
    for (int i = 0; i < default_encoding_menus_length; ++i) {
      int id = default_encoding_menus[i];
      // We have inserted this encoding, skip it.
      if (inserted_encoding.find(id) != inserted_encoding.end())
        continue;
      encoding_list->push_back(EncodingInfo(id));
    }
    // Sort the encoding list.
    l10n_util::SortVectorWithStringKey(locale,
                                       encoding_list,
                                       start_sorted_index,
                                       encoding_list->size(),
                                       true);
  }
  DCHECK(!encoding_list->empty());
  return encoding_list;
}

// Static
bool CharacterEncoding::UpdateRecentlySelectdEncoding(
    const std::wstring& original_selected_encodings,
    int new_selected_encoding_id,
    std::wstring* selected_encodings) {
  // Get encoding name.
  std::wstring encoding_name =
      GetCanonicalEncodingNameByCommandId(new_selected_encoding_id);
  DCHECK(!encoding_name.empty());
  // Check whether the new encoding is in local dependent encodings or original
  // recently selected encodings. If yes, do not add it.
  std::vector<int>* locale_dependent_encoding_list =
      canonical_encoding_name_map_singleton.locale_dependent_encoding_ids();
  DCHECK(locale_dependent_encoding_list);
  std::vector<int> selected_encoding_list;
  ParseEncodingListSeparatedWithComma(original_selected_encodings,
                                      &selected_encoding_list,
                                      kUserSelectedEncodingsMaxLength);
  // Put 'cached encodings' (dynamic encoding list) after 'local dependent
  // encoding list' for check.
  std::vector<int> top_encoding_list(*locale_dependent_encoding_list);
  // UTF8 is always in our optimized encoding list.
  top_encoding_list.insert(top_encoding_list.begin(), IDC_ENCODING_UTF8);
  top_encoding_list.insert(top_encoding_list.end(),
                           selected_encoding_list.begin(),
                           selected_encoding_list.end());
  for (std::vector<int>::const_iterator it = top_encoding_list.begin();
       it != top_encoding_list.end(); ++it)
    if (*it == new_selected_encoding_id)
      return false;
  // Need to add the encoding id to recently selected encoding list.
  // Remove the last encoding in original list.
  if (selected_encoding_list.size() == kUserSelectedEncodingsMaxLength)
    selected_encoding_list.pop_back();
  // Insert new encoding to head of selected encoding list.
  *selected_encodings = encoding_name;
  // Generate the string for rest selected encoding list.
  for (std::vector<int>::const_iterator it = selected_encoding_list.begin();
       it != selected_encoding_list.end(); ++it) {
    selected_encodings->append(1, L',');
    selected_encodings->append(GetCanonicalEncodingNameByCommandId(*it));
  }
  return true;
}
