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

#include "chrome/browser/encoding_menu_controller_delegate.h"

#include "chrome/app/chrome_dll_resource.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/character_encoding.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/tab_contents.h"
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
  std::string encoding_name = current_tab->GetEncoding();
  if (encoding_name.empty()) {
    encoding_name = WideToUTF8(
        profile->GetPrefs()->GetString(prefs::kDefaultCharset));
  }
  switch (id) {
    case IDC_ENCODING_AUTO_DETECT:
      return profile->GetPrefs()->GetBoolean(
          prefs::kWebKitUsesUniversalDetector);
    case IDC_ENCODING_UTF8:
    case IDC_ENCODING_UTF16LE:
    case IDC_ENCODING_ISO88591:
    case IDC_ENCODING_WINDOWS1252:
    case IDC_ENCODING_GB2312:
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
    case IDC_ENCODING_ISO88599:
    case IDC_ENCODING_WINDOWS1254:
    case IDC_ENCODING_ISO88596:
    case IDC_ENCODING_WINDOWS1256:
    case IDC_ENCODING_ISO88598:
    case IDC_ENCODING_WINDOWS1255:
    case IDC_ENCODING_WINDOWS1258:
      return (!encoding_name.empty() && encoding_name ==
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
