// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/encoding_menu_controller.h"

#include "app/l10n_util.h"
#include "chrome/app/chrome_dll_resource.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/character_encoding.h"
#include "chrome/browser/profile.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_service.h"
#include "grit/generated_resources.h"

const int EncodingMenuController::kValidEncodingIds[] = {
    IDC_ENCODING_UTF8,
    IDC_ENCODING_UTF16LE,
    IDC_ENCODING_ISO88591,
    IDC_ENCODING_WINDOWS1252,
    IDC_ENCODING_GBK,
    IDC_ENCODING_GB18030,
    IDC_ENCODING_BIG5,
    IDC_ENCODING_BIG5HKSCS,
    IDC_ENCODING_KOREAN,
    IDC_ENCODING_SHIFTJIS,
    IDC_ENCODING_ISO2022JP,
    IDC_ENCODING_EUCJP,
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
    IDC_ENCODING_ISO88594,
    IDC_ENCODING_ISO885913,
    IDC_ENCODING_WINDOWS1257,
    IDC_ENCODING_ISO88593,
    IDC_ENCODING_ISO885910,
    IDC_ENCODING_ISO885914,
    IDC_ENCODING_ISO885916,
    IDC_ENCODING_WINDOWS1254,
    IDC_ENCODING_ISO88596,
    IDC_ENCODING_WINDOWS1256,
    IDC_ENCODING_ISO88598,
    IDC_ENCODING_WINDOWS1255,
    IDC_ENCODING_WINDOWS1258,
    IDC_ENCODING_ISO88598I,
};

bool EncodingMenuController::DoesCommandBelongToEncodingMenu(int id) {
  if (id == IDC_ENCODING_AUTO_DETECT) {
    return true;
  }

  for (size_t i = 0; i < arraysize(kValidEncodingIds); ++i) {
    if (id == kValidEncodingIds[i]) {
      return true;
    }
  }

  return false;
}

const int* EncodingMenuController::ValidGUIEncodingIDs() {
  return kValidEncodingIds;
}

int EncodingMenuController::NumValidGUIEncodingIDs() {
  return arraysize(kValidEncodingIds);
}

bool EncodingMenuController::IsItemChecked(
    Profile* browser_profile,
    const std::wstring& current_tab_encoding,
    int item_id) {
  if (!DoesCommandBelongToEncodingMenu(item_id)) {
    return false;
  }

  std::wstring encoding = current_tab_encoding;
  if (encoding.empty()) {
    encoding = browser_profile->GetPrefs()->GetString(prefs::kDefaultCharset);
  }

  if (item_id == IDC_ENCODING_AUTO_DETECT) {
    return browser_profile->GetPrefs()->GetBoolean(
        prefs::kWebKitUsesUniversalDetector);
  }

  if (!encoding.empty()) {
    return encoding ==
        CharacterEncoding::GetCanonicalEncodingNameByCommandId(item_id);
  }

  return false;
}

void EncodingMenuController::GetEncodingMenuItems(Profile* profile,
    EncodingMenuItemList* menuItems) {

  DCHECK(menuItems);
  EncodingMenuItem separator(0, L"");

  menuItems->clear();
  menuItems->push_back(
      EncodingMenuItem(IDC_ENCODING_AUTO_DETECT,
                       l10n_util::GetString(IDS_ENCODING_AUTO_DETECT)));
  menuItems->push_back(separator);

  // Create current display encoding list.
  const std::vector<CharacterEncoding::EncodingInfo>* encodings;

  // Build the list of encoding ids : It is made of the
  // locale-dependent short list, the cache of recently selected
  // encodings and other encodings.
  encodings = CharacterEncoding::GetCurrentDisplayEncodings(
      g_browser_process->GetApplicationLocale(),
      profile->GetPrefs()->GetString(prefs::kStaticEncodings),
      profile->GetPrefs()->GetString(prefs::kRecentlySelectedEncoding));
  DCHECK(encodings);
  DCHECK(!encodings->empty());

  // Build up output list for menu.
  std::vector<CharacterEncoding::EncodingInfo>::const_iterator it;
  for (it = encodings->begin(); it != encodings->end(); ++it) {
    if (it->encoding_id) {
      std::wstring encoding = it->encoding_display_name;
      std::wstring bidi_safe_encoding;
      if (l10n_util::AdjustStringForLocaleDirection(encoding,
                                                    &bidi_safe_encoding))
        encoding.swap(bidi_safe_encoding);
      menuItems->push_back(EncodingMenuItem(it->encoding_id, encoding));
    } else {
      menuItems->push_back(separator);
    }
  }

}
