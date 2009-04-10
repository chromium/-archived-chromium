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

static bool GetIndexOfTabId(const TabStripModel* tab_strip, int tab_id,
                            int* tab_index);

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

  TabStripModel *tab_strip = browser->tabstrip_model();
  const DictionaryValue *args_hash = static_cast<const DictionaryValue*>(args_);

  // TODO(rafaelw): handle setting remaining tab properties:
  // -windowId
  // -title
  // -favIconUrl

  std::string url;
  args_hash->GetString(L"url", &url);

  // Default to foreground for the new tab. The presence of 'selected' property
  // will override this default.
  bool selected = true;
  args_hash->GetBoolean(L"selected", &selected);

  // If index is specified, honor the value, but keep it bound to
  // 0 <= index <= tab_strip->count()
  int index = -1;
  args_hash->GetInteger(L"index", &index);
  if (index < 0) {
    // Default insert behavior
    index = -1;
  }
  if (index > tab_strip->count()) {
    index = tab_strip->count();
  }

  TabContents* contents = browser->AddTabWithURL(GURL(url), GURL(),
      PageTransition::TYPED, selected, index, NULL);
  index = tab_strip->GetIndexOfTabContents(contents);

  // Return data about the newly created tab.
  if (has_callback())
    result_.reset(CreateTabValue(tab_strip, index));

  return true;
}

bool GetTabFunction::RunImpl() {
  if (!args_->IsType(Value::TYPE_INTEGER))
    return false;

  Browser* browser = BrowserList::GetLastActive();
  if (!browser)
    return false;

  int tab_id;
  args_->GetAsInteger(&tab_id);

  int tab_index;
  TabStripModel* tab_strip = browser->tabstrip_model();
  // TODO(rafaelw): return an error if the tab is not found by |tab_id|
  if (!GetIndexOfTabId(tab_strip, tab_id, &tab_index))
    return false;

  TabContents* tab_contents = tab_strip->GetTabContentsAt(tab_index);
  NavigationController* controller = tab_contents->controller();
  DCHECK(controller);
  result_.reset(CreateTabValue(tab_strip, tab_index));
  return true;
}

bool UpdateTabFunction::RunImpl() {
  // TODO(aa): Do data-driven validation in JS.
  if (!args_->IsType(Value::TYPE_DICTIONARY))
    return false;

  Browser* browser = BrowserList::GetLastActive();
  if (!browser)
    return false;

  int tab_id;
  const DictionaryValue *args_hash = static_cast<const DictionaryValue*>(args_);
  if (!args_hash->GetInteger(L"id", &tab_id))
    return false;

  int tab_index;
  TabStripModel* tab_strip = browser->tabstrip_model();
  // TODO(rafaelw): return an error if the tab is not found by |tab_id|
  if (!GetIndexOfTabId(tab_strip, tab_id, &tab_index))
    return false;

  TabContents* tab_contents = tab_strip->GetTabContentsAt(tab_index);
  NavigationController* controller = tab_contents->controller();
  DCHECK(controller);

  // TODO(rafaelw): handle setting remaining tab properties:
  // -index
  // -windowId
  // -title
  // -favIconUrl

  // Navigate the tab to a new location if the url different.
  std::string url;
  if (args_hash->GetString(L"url", &url)) {
    GURL new_gurl(url);
    if (new_gurl.is_valid()) {
      controller->LoadURL(new_gurl, GURL(), PageTransition::TYPED);
    } else {
      // TODO(rafaelw): return some reasonable error?
    }
  }

  bool selected;
  // TODO(rafaelw): Setting |selected| from js doesn't make much sense.
  // Move tab selection management up to window.
  if (args_hash->GetBoolean(L"selected", &selected) &&
      selected &&
      tab_strip->selected_index() != tab_index) {
    tab_strip->SelectTabContentsAt(tab_index, false);
  }

  return true;
}


bool RemoveTabFunction::RunImpl() {
  // TODO(rafaelw): This should have a callback, but it can't because it could
  // close it's own tab.

  if (!args_->IsType(Value::TYPE_INTEGER))
    return false;

  Browser* browser = BrowserList::GetLastActive();
  if (!browser)
    return false;

  int tab_id;
  if (!args_->GetAsInteger(&tab_id)) {
    return false;
  }

  int tab_index;
  TabStripModel* tab_strip = browser->tabstrip_model();
  if (GetIndexOfTabId(tab_strip, tab_id, &tab_index)) {
    TabContents* tab_contents = tab_strip->GetTabContentsAt(tab_index);
    NavigationController* controller = tab_contents->controller();
    DCHECK(controller);
    browser->CloseContents(tab_contents);
    return true;
  }

  return false;
}

// static helpers
static DictionaryValue* CreateTabValue(TabStripModel* tab_strip,
                                       int tab_index) {
  TabContents* contents = tab_strip->GetTabContentsAt(tab_index);
  NavigationController* controller = contents->controller();
  DCHECK(controller);  // TODO(aa): Is this a valid assumption?

  DictionaryValue* result = new DictionaryValue();
  result->SetInteger(L"id", controller->session_id().id());
  result->SetInteger(L"index", tab_index);
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

static bool GetIndexOfTabId(const TabStripModel* tab_strip, int tab_id,
                            int* tab_index) {
  for (int i = 0; i < tab_strip->count(); ++i) {
    TabContents* tab_contents = tab_strip->GetTabContentsAt(i);
    NavigationController* controller = tab_contents->controller();
    DCHECK(controller);  // TODO(aa): Is this a valid assumption?

    if (controller->session_id().id() == tab_id) {
      *tab_index = i;
      return true;
    }
  }
  return false;
}
