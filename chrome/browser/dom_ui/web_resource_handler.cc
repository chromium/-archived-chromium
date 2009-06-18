// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/values.h"
#include "chrome/browser/dom_ui/web_resource_handler.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/web_resource/web_resource_service.h"
#include "chrome/common/web_resource/web_resource_unpacker.h"
#include "chrome/common/pref_names.h"

namespace {

  const int kNumWebResourcesToShow = 2;

  // TODO(mrc): l10n
  // This title should only appear the very first time Chrome is run with
  // web resources enabled; otherwise the cache should be populated.
  static const wchar_t* kWebResourceTitleAtStartup =
      L"New: Suggestion Box!";

  // This snipp should only appear the very first time Chrome is run with
  // web resources enabled; otherwise the cache should be populated.
  static const wchar_t* kWebResourceSnippetAtStartup =
      L"Tips and recommendations to help you discover interesting websites.";
}

WebResourceHandler::WebResourceHandler(DOMUI* dom_ui)
    : DOMMessageHandler(dom_ui),
      dom_ui_(dom_ui) {
  dom_ui->RegisterMessageCallback("getNextCachedWebResource",
      NewCallback(this, &WebResourceHandler::HandleGetCachedWebResource));

  web_resource_cache_ = dom_ui_->GetProfile()->GetPrefs()->
      GetDictionary(prefs::kNTPWebResourceCache);
}

void WebResourceHandler::HandleGetCachedWebResource(const Value* content) {
  // Eventually we will feed more than one web resource datum at a time
  // to the NTP; for now, this is a list containing one item: the tip
  // to be displayed.
  ListValue list_value;

  // Holds the web resource data found in the preferences cache.
  DictionaryValue* wr_dict;

  // Dictionary which will be sent back in a Javascript call.
  DictionaryValue* tip_dict = new DictionaryValue();

  // These values hold the data for each web resource item.  As the web
  // resource server solidifies, these may change.
  std::wstring title;
  std::wstring thumb;
  std::wstring source;
  std::wstring snipp;
  std::wstring url;

  // This should only be true on the very first Chrome run; otherwise,
  // the cache should be populated.
  if (web_resource_cache_ == NULL || web_resource_cache_->GetSize() < 1) {
    title = kWebResourceTitleAtStartup;
    snipp = kWebResourceSnippetAtStartup;
  } else {
    // Right now, hard-coded to simply get the first item (marked "0") in the
    // resource data stored in the cache.  Fail silently if data is missing.
    // TODO(mrc): If data is missing, iterate through cache.
    web_resource_cache_->GetDictionary(L"0", &wr_dict);
    if (wr_dict &&
        wr_dict->GetSize() > 0 &&
        wr_dict->GetString(WebResourceService::kWebResourceTitle, &title) &&
        wr_dict->GetString(WebResourceService::kWebResourceThumb, &thumb) &&
        wr_dict->GetString(WebResourceService::kWebResourceSource, &source) &&
        wr_dict->GetString(WebResourceService::kWebResourceSnippet, &snipp) &&
        wr_dict->GetString(WebResourceService::kWebResourceURL, &url)) {
      tip_dict->SetString(WebResourceService::kWebResourceTitle, title);
      tip_dict->SetString(WebResourceService::kWebResourceThumb, thumb);
      tip_dict->SetString(WebResourceService::kWebResourceSource, source);
      tip_dict->SetString(WebResourceService::kWebResourceSnippet, snipp);
      tip_dict->SetString(WebResourceService::kWebResourceURL, url);
    }
  }

  list_value.Append(tip_dict);

  // Send list of snippets back out to the DOM.
  dom_ui_->CallJavascriptFunction(L"nextWebResource", list_value);
}

// static
void WebResourceHandler::RegisterUserPrefs(PrefService* prefs) {
  prefs->RegisterDictionaryPref(prefs::kNTPWebResourceCache);
  prefs->RegisterStringPref(prefs::kNTPWebResourceServer,
      WebResourceService::kDefaultResourceServer);
}

