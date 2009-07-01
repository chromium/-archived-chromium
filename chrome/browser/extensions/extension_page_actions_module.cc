// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_page_actions_module.h"

#include "chrome/browser/browser.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/extensions/extension_page_actions_module_constants.h"
#include "chrome/browser/extensions/extension_tabs_module.h"
#include "chrome/browser/extensions/extensions_service.h"
#include "chrome/browser/tab_contents/navigation_entry.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_error_utils.h"

namespace keys = extension_page_actions_module_constants;

bool PageActionFunction::SetPageActionEnabled(bool enable) {
  EXTENSION_FUNCTION_VALIDATE(args_->IsType(Value::TYPE_LIST));
  const ListValue* args = static_cast<const ListValue*>(args_);

  std::string page_action_id;
  EXTENSION_FUNCTION_VALIDATE(args->GetString(0, &page_action_id));
  DictionaryValue* action;
  EXTENSION_FUNCTION_VALIDATE(args->GetDictionary(1, &action));

  int tab_id;
  EXTENSION_FUNCTION_VALIDATE(action->GetInteger(keys::kTabIdKey, &tab_id));
  std::string url;
  EXTENSION_FUNCTION_VALIDATE(action->GetString(keys::kUrlKey, &url));

  std::string title;
  int icon_id = 0;
  if (enable) {
    // Both of those are optional.
    if (action->HasKey(keys::kTitleKey))
      EXTENSION_FUNCTION_VALIDATE(action->GetString(keys::kTitleKey, &title));
    if (action->HasKey(keys::kIconIdKey)) {
      EXTENSION_FUNCTION_VALIDATE(action->GetInteger(keys::kIconIdKey,
                                                     &icon_id));
    }
  }

  // Find the TabContents that contains this tab id.
  TabContents* contents = NULL;
  ExtensionTabUtil::GetTabById(tab_id, profile(), NULL, NULL, &contents, NULL);
  if (!contents) {
    error_ = ExtensionErrorUtils::FormatErrorMessage(keys::kNoTabError,
                                                     IntToString(tab_id));
    return false;
  }

  // Make sure the URL hasn't changed.
  NavigationEntry* entry = contents->controller().GetActiveEntry();
  if (!entry || url != entry->url().spec()) {
    error_ = ExtensionErrorUtils::FormatErrorMessage(keys::kUrlNotActiveError,
                                                     url);
    return false;
  }

  // Find our extension.
  Extension* extension = NULL;
  ExtensionsService* service = profile()->GetExtensionsService();
  extension = service->GetExtensionById(extension_id());
  if (!extension) {
    error_ = ExtensionErrorUtils::FormatErrorMessage(keys::kNoExtensionError,
                                                     extension_id());
    return false;
  }

  const PageAction* page_action = extension->GetPageAction(page_action_id);
  if (!page_action) {
    error_ = ExtensionErrorUtils::FormatErrorMessage(keys::kNoPageActionError,
                                                     page_action_id);
    return false;
  }

  // Set visibility and broadcast notifications that the UI should be updated.
  contents->SetPageActionEnabled(page_action, enable, title, icon_id);
  contents->NotifyNavigationStateChanged(TabContents::INVALIDATE_PAGE_ACTIONS);

  return true;
}

bool EnablePageActionFunction::RunImpl() {
  return SetPageActionEnabled(true);
}

bool DisablePageActionFunction::RunImpl() {
  return SetPageActionEnabled(false);
}
