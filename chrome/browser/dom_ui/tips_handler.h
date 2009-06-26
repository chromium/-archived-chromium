// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This class pulls data from a web resource (such as a JSON feed) which
// has been stored in the user's preferences file.  Used mainly
// by the suggestions and tips area of the new tab page.

// Current sketch of tip cache format, hardcoded for popgadget data in
// basic text form:

// "tip_cache": {
//    "0": {
//        "index": should become time field (or not)
//        "snippet": the text of the item
//        "source": text describing source (i.e., "New York Post")
//        "thumbnail": URL of thumbnail on popgadget server
//        "title": text giving title of item
//        "url": link to item's page
//    },
//    [up to number of items in kMaxWebResourceCacheSize]

#ifndef CHROME_BROWSER_DOM_UI_TIPS_HANDLER_H_
#define CHROME_BROWSER_DOM_UI_TIPS_HANDLER_H_

#include "chrome/browser/dom_ui/dom_ui.h"

class DictionaryValue;
class DOMUI;
class PrefService;
class Value;

class TipsHandler : public DOMMessageHandler {
 public:
  TipsHandler() : tips_cache_(NULL) { }
  virtual ~TipsHandler() { }

  // DOMMessageHandler implementation and overrides. 
  virtual DOMMessageHandler* Attach(DOMUI* dom_ui); 
  virtual void RegisterMessages();

  // Callback which pulls tips data from the preferences.
  void HandleGetTips(const Value* content);

  // Register tips cache with pref service.
  static void RegisterUserPrefs(PrefService* prefs);

 private:
  // Filled with data from cache in preferences.
  const DictionaryValue* tips_cache_;

  DISALLOW_COPY_AND_ASSIGN(TipsHandler);
};

#endif  // CHROME_BROWSER_DOM_UI_TIPS_HANDLER_H_

