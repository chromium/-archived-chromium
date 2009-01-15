// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/encoding_menu_controller_delegate.h"

#include "chrome/app/chrome_dll_resource.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/character_encoding.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/tab_contents/web_contents.h"
#include "chrome/common/l10n_util.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_service.h"

#include "generated_resources.h"

EncodingMenuControllerDelegate::EncodingMenuControllerDelegate(
    Browser* browser, Controller* wrapped)
    : BaseControllerDelegate(wrapped),
      browser_(browser) {
}

bool EncodingMenuControllerDelegate::IsItemChecked(int id) const {
  Profile* profile = browser_->profile();
  if (!profile)
    return false;
  TabContents* current_tab = browser_->GetSelectedTabContents();
  if (!current_tab)
    return false;
  std::wstring encoding;
  if (current_tab->AsWebContents())
    encoding = current_tab->AsWebContents()->encoding();
  if (encoding.empty())
    encoding = profile->GetPrefs()->GetString(prefs::kDefaultCharset);
  switch (id) {
    case IDC_ENCODING_AUTO_DETECT:
      return profile->GetPrefs()->GetBoolean(
          prefs::kWebKitUsesUniversalDetector);
    case IDC_ENCODING_UTF8:
    case IDC_ENCODING_UTF16LE:
    case IDC_ENCODING_ISO88591:
    case IDC_ENCODING_WINDOWS1252:
    case IDC_ENCODING_GBK:
    case IDC_ENCODING_GB18030:
    case IDC_ENCODING_BIG5:
    case IDC_ENCODING_BIG5HKSCS:
    case IDC_ENCODING_KOREAN:
    case IDC_ENCODING_SHIFTJIS:
    case IDC_ENCODING_ISO2022JP:
    case IDC_ENCODING_EUCJP:
    case IDC_ENCODING_THAI:
    case IDC_ENCODING_ISO885915:
    case IDC_ENCODING_MACINTOSH:
    case IDC_ENCODING_ISO88592:
    case IDC_ENCODING_WINDOWS1250:
    case IDC_ENCODING_ISO88595:
    case IDC_ENCODING_WINDOWS1251:
    case IDC_ENCODING_KOI8R:
    case IDC_ENCODING_KOI8U:
    case IDC_ENCODING_ISO88597:
    case IDC_ENCODING_WINDOWS1253:
    case IDC_ENCODING_ISO88594:
    case IDC_ENCODING_ISO885913:
    case IDC_ENCODING_WINDOWS1257:
    case IDC_ENCODING_ISO88593:
    case IDC_ENCODING_ISO885910:
    case IDC_ENCODING_ISO885914:
    case IDC_ENCODING_ISO885916:
    case IDC_ENCODING_WINDOWS1254:
    case IDC_ENCODING_ISO88596:
    case IDC_ENCODING_WINDOWS1256:
    case IDC_ENCODING_ISO88598:
    case IDC_ENCODING_WINDOWS1255:
    case IDC_ENCODING_WINDOWS1258:
      return (!encoding.empty() && encoding ==
          CharacterEncoding::GetCanonicalEncodingNameByCommandId(id));
    default:
      return false;
  }
}

void EncodingMenuControllerDelegate::BuildEncodingMenu(
    Profile* profile, Menu* encoding_menu) {
  // Append auto-detection item.
  encoding_menu->AppendMenuItem(IDC_ENCODING_AUTO_DETECT,
                                l10n_util::GetString(IDS_ENCODING_AUTO_DETECT),
                                Menu::CHECKBOX);

  // Append encoding item.
  encoding_menu->AppendSeparator();
  // Create current display encoding list.
  std::wstring cur_locale = g_browser_process->GetApplicationLocale();
  const std::vector<int>* encoding_ids;
  // Build the list of encoding ids : It is made of the
  // locale-dependent short list, the cache of recently selected
  // encodings and other encodings.
  encoding_ids = CharacterEncoding::GetCurrentDisplayEncodings(
      profile->GetPrefs()->GetString(prefs::kStaticEncodings),
      profile->GetPrefs()->GetString(prefs::kRecentlySelectedEncoding));
  DCHECK(encoding_ids);
  DCHECK(!encoding_ids->empty());
  unsigned len = static_cast<unsigned>(encoding_ids->size());
  // Add encoding menus.
  std::vector<int>::const_iterator it;
  for (it = encoding_ids->begin(); it != encoding_ids->end(); ++it) {
    if (*it) {
      std::wstring encoding =
        CharacterEncoding::GetCanonicalEncodingDisplayNameByCommandId(*it);
      std::wstring bidi_safe_encoding;
      if (l10n_util::AdjustStringForLocaleDirection(encoding,
                                                    &bidi_safe_encoding))
        encoding.swap(bidi_safe_encoding);
      encoding_menu->AppendMenuItem(*it, encoding, Menu::RADIO);
    }
    else
      encoding_menu->AppendSeparator();
  }
}

