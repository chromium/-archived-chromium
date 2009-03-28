// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_tabs_module.h"

#include "base/string_util.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/extensions/extension_function_dispatcher.h"
#include "chrome/browser/tab_contents/navigation_entry.h"

// Forward declare static helper functions defined below.
static DictionaryValue* CreateTabValue(TabStripModel* tab_strip_model,
                                       int tab_index);

bool GetTabsForWindowFunction::RunImpl() {
  if (!args_->IsType(Value::TYPE_NULL))
    return false;

  Browser* browser = BrowserList::GetLastActive();
  if (!browser)
    return false;

  TabStripModel* tab_strip = browser->tabstrip_model();
  result_.reset(new ListValue());
  for (int i = 0; i < tab_strip->count(); ++i) {
    static_cast<ListValue*>(result_.get())->Append(
        CreateTabValue(tab_strip, i));
  }

  return true;
}

bool CreateTabFunction::RunImpl() {
  // TODO(aa): Do data-driven validation in JS.
  if (!args_->IsType(Value::TYPE_DICTIONARY))
    return false;

  Browser* browser = BrowserList::GetLastActive();
  if (!browser)
    return false;

  // TODO(aa): Handle all the other properties of the new tab.
  std::string url;
  static_cast<const DictionaryValue*>(args_)->GetString(L"url", &url);
  browser->AddTabWithURL(GURL(url), GURL(), PageTransition::TYPED, true, NULL);

  // Return data about the newly created tab.
  if (has_callback())
    result_.reset(CreateTabValue(browser->tabstrip_model(),
                                 browser->tabstrip_model()->count() - 1));

  return true;
}


// static helpers
static DictionaryValue* CreateTabValue(TabStripModel* tab_strip,
                                       int tab_index) {
  TabContents* contents = tab_strip->GetTabContentsAt(tab_index);
  NavigationController* controller = contents->controller();
  DCHECK(controller);  // TODO(aa): Is this a valid assumption?

  DictionaryValue* result = new DictionaryValue();
  result->SetInteger(L"id", controller->session_id().id());
  result->SetInteger(L"windowId", controller->window_id().id());
  result->SetString(L"url", contents->GetURL().spec());
  result->SetString(L"title", UTF16ToWide(contents->GetTitle()));
  result->SetBoolean(L"selected", tab_index == tab_strip->selected_index());

  NavigationEntry* entry = controller->GetActiveEntry();
  if (entry) {
    if (entry->favicon().is_valid())
      result->SetString(L"favIconUrl", entry->favicon().url().spec());
  }

  return result;
}
