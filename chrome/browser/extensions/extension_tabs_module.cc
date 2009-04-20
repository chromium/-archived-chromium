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
static DictionaryValue* CreateWindowValue(Browser* browser);
static ListValue* CreateTabList(Browser* browser);
static DictionaryValue* CreateTabValue(TabStripModel* tab_strip_model,
                                       int tab_index);
static bool GetIndexOfTabId(const TabStripModel* tab_strip, int tab_id,
                            int* tab_index);

// ExtensionTabUtil
int ExtensionTabUtil::GetWindowId(const Browser* browser) {
  return browser->session_id().id();
}

int ExtensionTabUtil::GetTabId(const TabContents* tab_contents) {
  return tab_contents->controller().session_id().id();
}

int ExtensionTabUtil::GetWindowIdOfTab(const TabContents* tab_contents) {
  return tab_contents->controller().window_id().id();
}

bool GetWindowsFunction::RunImpl() {
  std::set<int> window_ids;

  // Look for |ids| named parameter as list of id's to fetch.
  if (args_->IsType(Value::TYPE_DICTIONARY)) {
    Value *ids_value;
    if ((!static_cast<DictionaryValue*>(args_)->Get(L"ids", &ids_value)) ||
        (!ids_value->IsType(Value::TYPE_LIST))) {
      DCHECK(false);
      return false;
    }

    ListValue *window_id_list = static_cast<ListValue*>(ids_value);
    for (ListValue::iterator id = window_id_list->begin();
         id != window_id_list->end(); ++id) {
      int window_id;
      if (!(*id)->GetAsInteger(&window_id)) {
        DCHECK(false);
        return false;
      }

      window_ids.insert(window_id);
    }
  }

  // Default to all windows.
  bool all_windows = (window_ids.size() == 0);

  result_.reset(new ListValue());
  for (BrowserList::const_iterator browser = BrowserList::begin();
       browser != BrowserList::end(); ++browser) {
    if (all_windows || (window_ids.find(ExtensionTabUtil::GetWindowId(*browser))
        != window_ids.end())) {
      static_cast<ListValue*>(result_.get())->Append(
        CreateWindowValue(*browser));
    }
  }

  return true;
}

bool GetTabsForWindowFunction::RunImpl() {
  if (!args_->IsType(Value::TYPE_NULL))
    return false;

  Browser* browser = BrowserList::GetLastActive();
  if (!browser)
    return false;

  result_.reset(CreateTabList(browser));

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
  const DictionaryValue *args = static_cast<const DictionaryValue*>(args_);

  // TODO(rafaelw): handle setting remaining tab properties:
  // -windowId
  // -title
  // -favIconUrl

  std::string url;
  args->GetString(L"url", &url);

  // Default to foreground for the new tab. The presence of 'selected' property
  // will override this default.
  bool selected = true;
  args->GetBoolean(L"selected", &selected);

  // If index is specified, honor the value, but keep it bound to
  // 0 <= index <= tab_strip->count()
  int index = -1;
  args->GetInteger(L"index", &index);
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
  const DictionaryValue *args = static_cast<const DictionaryValue*>(args_);
  if (!args->GetInteger(L"id", &tab_id))
    return false;

  int tab_index;
  TabStripModel* tab_strip = browser->tabstrip_model();
  // TODO(rafaelw): return an error if the tab is not found by |tab_id|
  if (!GetIndexOfTabId(tab_strip, tab_id, &tab_index))
    return false;

  TabContents* tab_contents = tab_strip->GetTabContentsAt(tab_index);
  NavigationController& controller = tab_contents->controller();

  // TODO(rafaelw): handle setting remaining tab properties:
  // -title
  // -favIconUrl

  // Navigate the tab to a new location if the url different.
  std::string url;
  if (args->GetString(L"url", &url)) {
    GURL new_gurl(url);
    if (new_gurl.is_valid()) {
      controller.LoadURL(new_gurl, GURL(), PageTransition::TYPED);
    } else {
      // TODO(rafaelw): return some reasonable error?
    }
  }

  bool selected;
  // TODO(rafaelw): Setting |selected| from js doesn't make much sense.
  // Move tab selection management up to window.
  if (args->GetBoolean(L"selected", &selected) &&
      selected &&
      tab_strip->selected_index() != tab_index) {
    tab_strip->SelectTabContentsAt(tab_index, false);
  }

  return true;
}

bool MoveTabFunction::RunImpl() {
  if (!args_->IsType(Value::TYPE_DICTIONARY))
    return false;

  Browser* browser = BrowserList::GetLastActive();
  if (!browser)
    return false;

  int tab_id;
  const DictionaryValue *args = static_cast<const DictionaryValue*>(args_);
  if (!args->GetInteger(L"id", &tab_id))
    return false;

  int tab_index;
  TabStripModel* tab_strip = browser->tabstrip_model();
  // TODO(rafaelw): return an error if the tab is not found by |tab_id|
  if (!GetIndexOfTabId(tab_strip, tab_id, &tab_index))
    return false;

  // TODO(rafaelw): support moving tabs between windows
  // -windowId

  int new_index;
  DCHECK(args->GetInteger(L"index", &new_index));
  if (new_index < 0) {
    DCHECK(false);
    return false;
  }

  // Clamp move location to the last position.
  if (new_index >= tab_strip->count()) {
    new_index = tab_strip->count() - 1;
  }

  if (new_index != tab_index) {
    tab_strip->MoveTabContentsAt(tab_index, new_index, false);
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
    browser->CloseContents(tab_strip->GetTabContentsAt(tab_index));
    return true;
  }

  return false;
}

// static helpers
static DictionaryValue* CreateWindowValue(Browser* browser) {
  DictionaryValue* result = new DictionaryValue();
  result->SetInteger(L"id", ExtensionTabUtil::GetWindowId(browser));
  result->SetBoolean(L"focused", browser->window()->IsActive());
  gfx::Rect bounds = browser->window()->GetNormalBounds();

  // TODO(rafaelw): zIndex ?
  result->SetInteger(L"left", bounds.x());
  result->SetInteger(L"top", bounds.y());
  result->SetInteger(L"width", bounds.width());
  result->SetInteger(L"height", bounds.height());

  result->Set(L"tabs", CreateTabList(browser));

  return result;
}

static ListValue* CreateTabList(Browser* browser) {
  ListValue *tab_list = new ListValue();
  TabStripModel* tab_strip = browser->tabstrip_model();
  for (int i = 0; i < tab_strip->count(); ++i) {
    tab_list->Append(CreateTabValue(tab_strip, i));
  }

  return tab_list;
}

static DictionaryValue* CreateTabValue(TabStripModel* tab_strip,
                                       int tab_index) {
  TabContents* contents = tab_strip->GetTabContentsAt(tab_index);

  DictionaryValue* result = new DictionaryValue();
  result->SetInteger(L"id", ExtensionTabUtil::GetTabId(contents));
  result->SetInteger(L"index", tab_index);
  result->SetInteger(L"windowId", ExtensionTabUtil::GetWindowIdOfTab(contents));
  result->SetString(L"url", contents->GetURL().spec());
  result->SetString(L"title", UTF16ToWide(contents->GetTitle()));
  result->SetBoolean(L"selected", tab_index == tab_strip->selected_index());

  NavigationEntry* entry = contents->controller().GetActiveEntry();
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
    if (tab_contents->controller().session_id().id() == tab_id) {
      *tab_index = i;
      return true;
    }
  }
  return false;
}
