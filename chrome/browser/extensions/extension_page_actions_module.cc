// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_page_actions_module.h"

#include "chrome/browser/browser.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/extensions/extension.h"
#include "chrome/browser/extensions/extension_tabs_module.h"
#include "chrome/browser/extensions/extensions_service.h"

bool EnablePageActionFunction::RunImpl() {
  EXTENSION_FUNCTION_VALIDATE(args_->IsType(Value::TYPE_LIST));
  const ListValue* args = static_cast<const ListValue*>(args_);

  std::string page_action_id;
  EXTENSION_FUNCTION_VALIDATE(args->GetString(0, &page_action_id));
  DictionaryValue* action;
  EXTENSION_FUNCTION_VALIDATE(args->GetDictionary(1, &action));

  int tab_id;
  EXTENSION_FUNCTION_VALIDATE(action->GetInteger(L"tabId", &tab_id));
  std::string url;
  EXTENSION_FUNCTION_VALIDATE(action->GetString(L"url", &url));

  Browser* browser = BrowserList::GetLastActive();
  if (!browser)
    return false;

  // HACK: We need to figure out the tab index from the tab_id (pending).
  // For now we only support page actions in the first tab in the strip (tab 0).
  int tab_index = 0;

  TabStripModel* tab_strip = browser->tabstrip_model();
  TabContents* contents = tab_strip->GetTabContentsAt(tab_index);

  // Not needed when we stop hard-coding the tab index.
  tab_id = ExtensionTabUtil::GetTabId(contents);

  // Find our extension.
  Extension* extension = NULL;
  if (profile()->GetExtensionsService()) {
    const ExtensionList* extensions =
        profile()->GetExtensionsService()->extensions();
    for (ExtensionList::const_iterator iter = extensions->begin();
        iter != extensions->end(); ++iter) {
      if ((*iter)->id() == extension_id()) {
        extension = (*iter);
        break;  // Found our extension.
      }
    }
  }

  if (!extension ||
      !extension->UpdatePageAction(page_action_id, tab_id, GURL(url)))
    return false;

  // Broadcast notifications when the UI should be updated.
  contents->NotifyNavigationStateChanged(TabContents::INVALIDATE_PAGE_ACTIONS);

  return true;
}
