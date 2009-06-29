// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/string_util.h"
#include "base/values.h"
#include "chrome/browser/dom_ui/tips_handler.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/web_resource/web_resource_service.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/web_resource/web_resource_unpacker.h"
#include "chrome/common/url_constants.h"
#include "googleurl/src/gurl.h"

namespace {

  // TODO(mrc): l10n
  // This title should only appear the very first time Chrome is run with
  // web resources enabled; otherwise the cache should be populated.
  static const wchar_t* kTipsTitleAtStartup =
      L"Tips and recommendations to help you discover interesting websites.";
}

TipsHandler::TipsHandler(DOMUI* dom_ui)
    : DOMMessageHandler(dom_ui),
      dom_ui_(dom_ui) {
  dom_ui->RegisterMessageCallback("getTips",
      NewCallback(this, &TipsHandler::HandleGetTips));

  tips_cache_ = dom_ui_->GetProfile()->GetPrefs()->
      GetDictionary(prefs::kNTPTipsCache);
}

void TipsHandler::HandleGetTips(const Value* content) {
  // List containing the tips to be displayed.
  ListValue list_value;

  // Holds the web resource data found in the preferences cache.
  DictionaryValue* wr_dict;

  // These values hold the data for each web resource item.  As the web
  // resource server solidifies, these may change.
  std::wstring title;
  std::wstring thumb;
  std::wstring source;
  std::wstring snipp;
  std::wstring url;

  // This should only be true on the very first Chrome run; otherwise,
  // the cache should be populated.
  if (tips_cache_ == NULL || tips_cache_->GetSize() < 1) {
    title = kTipsTitleAtStartup;
    DictionaryValue* tip_dict = new DictionaryValue();
    tip_dict->SetString(WebResourceService::kWebResourceTitle, title);
    tip_dict->SetString(WebResourceService::kWebResourceURL, L"");
    list_value.Append(tip_dict);
  } else {
    int tip_counter = 0;
    while (tips_cache_->GetDictionary(IntToWString(tip_counter++), &wr_dict)) {
      if (wr_dict &&
          wr_dict->GetSize() > 0 &&
          wr_dict->GetString(WebResourceService::kWebResourceTitle, &title) &&
          wr_dict->GetString(WebResourceService::kWebResourceURL, &url) &&
          IsValidURL(url)) {
        DictionaryValue* tip_dict = new DictionaryValue();
        tip_dict->SetString(WebResourceService::kWebResourceTitle, title);
        tip_dict->SetString(WebResourceService::kWebResourceURL, url);
        list_value.Append(tip_dict);
      }
    }
  }

  // Send list of web resource items back out to the DOM.
  dom_ui_->CallJavascriptFunction(L"tips", list_value);
}

// static
void TipsHandler::RegisterUserPrefs(PrefService* prefs) {
  prefs->RegisterDictionaryPref(prefs::kNTPTipsCache);
  prefs->RegisterStringPref(prefs::kNTPTipsServer,
                            WebResourceService::kDefaultResourceServer);
}

bool TipsHandler::IsValidURL(const std::wstring& url_string) {
  GURL url(WideToUTF8(url_string));
  return !url.is_empty() && (url.SchemeIs(chrome::kHttpScheme) ||
                             url.SchemeIs(chrome::kHttpsScheme));
}

