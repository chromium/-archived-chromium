// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dom_ui/shown_sections_handler.h"

#include "base/string_util.h"
#include "base/values.h"
#include "chrome/browser/profile.h"
#include "chrome/common/pref_names.h"

void ShownSectionsHandler::RegisterMessages() {
  dom_ui_->RegisterMessageCallback("getShownSections",
      NewCallback(this, &ShownSectionsHandler::HandleGetShownSections));
  dom_ui_->RegisterMessageCallback("setShownSections",
      NewCallback(this, &ShownSectionsHandler::HandleSetShownSections));
}

void ShownSectionsHandler::HandleGetShownSections(const Value* value) {
  const int mode = dom_ui_->GetProfile()->GetPrefs()->
      GetInteger(prefs::kNTPShownSections);
  FundamentalValue* mode_value = new FundamentalValue(mode);
  dom_ui_->CallJavascriptFunction(L"onShownSections", *mode_value);
}

void ShownSectionsHandler::HandleSetShownSections(const Value* value) {
  if (!value->IsType(Value::TYPE_LIST)) {
    NOTREACHED();
    return;
  }

  const ListValue* list = static_cast<const ListValue*>(value);
  std::string mode_string;

  if (list->GetSize() < 1) {
    NOTREACHED() << "setShownSections called with too few arguments";
    return;
  }

  bool r = list->GetString(0, &mode_string);
  DCHECK(r) << "Missing value in setShownSections from the NTP Most Visited.";

  dom_ui_->GetProfile()->GetPrefs()->SetInteger(
      prefs::kNTPShownSections, StringToInt(mode_string));
}

// static
void ShownSectionsHandler::RegisterUserPrefs(PrefService* prefs) {
  prefs->RegisterIntegerPref(prefs::kNTPShownSections,
      THUMB | RECENT | TIPS);
}

