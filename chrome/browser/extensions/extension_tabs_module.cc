// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_tabs_module.h"

#include "base/string_util.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/extensions/extension_function_dispatcher.h"
#include "chrome/browser/renderer_host/render_view_host_delegate.h"
#include "chrome/browser/tab_contents/navigation_entry.h"

// TODO(port): Port these files.
#if defined(OS_WIN)
#include "chrome/browser/window_sizer.h"
#else
#include "chrome/common/temp_scaffolding_stubs.h"
#endif

// Forward declare static helper functions defined below.
static DictionaryValue* CreateWindowValue(Browser* browser);
static ListValue* CreateTabList(Browser* browser);
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

DictionaryValue* ExtensionTabUtil::CreateTabValue(
    const TabContents* contents) {
  // Find the tab strip and index of this guy.
  for (BrowserList::const_iterator it = BrowserList::begin();
      it != BrowserList::end(); ++it) {
    TabStripModel* tab_strip = (*it)->tabstrip_model();
    int tab_index = tab_strip->GetIndexOfTabContents(contents);
    if (tab_index != -1) {
      return ExtensionTabUtil::CreateTabValue(contents, tab_strip, tab_index);
    }
  }

  // Couldn't find it.  This can happen if the tab is being dragged.
  return ExtensionTabUtil::CreateTabValue(contents, NULL, -1);
}

DictionaryValue* ExtensionTabUtil::CreateTabValue(
    const TabContents* contents, TabStripModel* tab_strip, int tab_index) {
  DictionaryValue* result = new DictionaryValue();
  result->SetInteger(L"id", ExtensionTabUtil::GetTabId(contents));
  result->SetInteger(L"index", tab_index);
  result->SetInteger(L"windowId", ExtensionTabUtil::GetWindowIdOfTab(contents));
  result->SetString(L"url", contents->GetURL().spec());
  result->SetString(L"title", UTF16ToWide(contents->GetTitle()));
  result->SetBoolean(L"selected",
                     tab_strip && tab_index == tab_strip->selected_index());

  NavigationEntry* entry = contents->controller().GetActiveEntry();
  if (entry) {
    if (entry->favicon().is_valid())
      result->SetString(L"favIconUrl", entry->favicon().url().spec());
  }

  return result;
}

bool GetWindowsFunction::RunImpl() {
  std::set<int> window_ids;

  // Look for |ids| named parameter as list of id's to fetch.
  if (args_->IsType(Value::TYPE_DICTIONARY)) {
    ListValue *window_id_list;
    const DictionaryValue *args = static_cast<const DictionaryValue*>(args_);
    EXTENSION_FUNCTION_VALIDATE(args->GetList(L"ids", &window_id_list));
    for (ListValue::iterator id = window_id_list->begin();
         id != window_id_list->end(); ++id) {
      int window_id;
      EXTENSION_FUNCTION_VALIDATE((*id)->GetAsInteger(&window_id));
      window_ids.insert(window_id);
    }
  }

  // Default to all windows.
  bool all_windows = (window_ids.size() == 0);

  result_.reset(new ListValue());
  for (BrowserList::const_iterator browser = BrowserList::begin();
       browser != BrowserList::end(); ++browser) {
    // Only examine browsers in the current profile.
    if ((*browser)->profile() == profile()) {
      if (all_windows || (window_ids.find(ExtensionTabUtil::
          GetWindowId(*browser)) != window_ids.end())) {
        static_cast<ListValue*>(result_.get())->Append(
        CreateWindowValue(*browser));
      }
    }
  }

  return true;
}

bool CreateWindowFunction::RunImpl() {
  scoped_ptr<GURL> url(new GURL());

  // Look for optional url.
  if (args_->IsType(Value::TYPE_DICTIONARY)) {
    const DictionaryValue *args = static_cast<const DictionaryValue*>(args_);
    std::string url_input;
    if (args->GetString(L"url", &url_input)) {
      url.reset(new GURL(url_input));
      if (!url->is_valid()) {
        // TODO(rafaelw): need error message/callback here
        return false;
      }
    }
  }

  // Try to get the browser associated with view that this call came from, so
  // its position can be set relative to its browser window.
  Browser* browser = dispatcher_->browser();
  if (browser == NULL)
    browser = BrowserList::GetLastActiveWithProfile(dispatcher_->profile());

  // Try to position the new browser relative its originating browser window.
  gfx::Rect empty_bounds;
  gfx::Rect bounds;
  bool maximized;
  // The call offsets the bounds by kWindowTilePixels (defined in WindowSizer to
  // be 10).
  WindowSizer::GetBrowserWindowBounds(std::wstring(), empty_bounds, browser,
      &bounds, &maximized);

  // Any part of the bounds can optionally be set by the caller.
  if (args_->IsType(Value::TYPE_DICTIONARY)) {
    const DictionaryValue *args = static_cast<const DictionaryValue*>(args_);
    int bounds_val;
    if (args->GetInteger(L"left", &bounds_val))
      bounds.set_x(bounds_val);

    if (args->GetInteger(L"top", &bounds_val))
      bounds.set_y(bounds_val);

    if (args->GetInteger(L"width", &bounds_val))
      bounds.set_width(bounds_val);

    if (args->GetInteger(L"height", &bounds_val))
      bounds.set_height(bounds_val);
  }

  Browser *new_window = Browser::Create(dispatcher_->profile());
  if (url->is_valid()) {
    new_window->AddTabWithURL(*(url.get()),
                              GURL(), PageTransition::LINK,
                              true, -1, NULL);
  } else {
    new_window->NewTab();
  }
  new_window->window()->SetBounds(bounds);
  new_window->window()->Show();

  // TODO(rafaelw): support |focused|, |zIndex|

  result_.reset(CreateWindowValue(new_window));

  return true;
}

bool RemoveWindowFunction::RunImpl() {
  int window_id;
  EXTENSION_FUNCTION_VALIDATE(args_->GetAsInteger(&window_id));

  Browser* target = NULL;
  for (BrowserList::const_iterator browser = BrowserList::begin();
      browser != BrowserList::end(); ++browser) {
    // Only examine browsers in the current profile.
    if ((*browser)->profile() == profile()) {
      if (ExtensionTabUtil::GetWindowId(*browser) == window_id) {
        target = *browser;
        break;
      }
    }
  }

  if (target == NULL) {
    // TODO(rafaelw): need error message.
    return false;
  }

  target->CloseWindow();

  return true;
}


bool GetTabsForWindowFunction::RunImpl() {
  EXTENSION_FUNCTION_VALIDATE(args_->IsType(Value::TYPE_NULL));

  Browser* browser = dispatcher_->browser();
  if (!browser)
    return false;

  result_.reset(CreateTabList(browser));

  return true;
}

bool CreateTabFunction::RunImpl() {
  EXTENSION_FUNCTION_VALIDATE(args_->IsType(Value::TYPE_DICTIONARY));
  const DictionaryValue *args = static_cast<const DictionaryValue*>(args_);

  Browser* browser = BrowserList::GetLastActive();
  if (!browser)
    return false;

  TabStripModel *tab_strip = browser->tabstrip_model();

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
      PageTransition::LINK, selected, index, NULL);
  index = tab_strip->GetIndexOfTabContents(contents);

  // Return data about the newly created tab.
  if (has_callback())
    result_.reset(ExtensionTabUtil::CreateTabValue(contents, tab_strip, index));

  return true;
}

bool GetTabFunction::RunImpl() {
  int tab_id;
  EXTENSION_FUNCTION_VALIDATE(args_->GetAsInteger(&tab_id));

  Browser* browser = BrowserList::GetLastActive();
  if (!browser)
    return false;

  int tab_index;
  TabStripModel* tab_strip = browser->tabstrip_model();
  // TODO(rafaelw): return an error if the tab is not found by |tab_id|
  if (!GetIndexOfTabId(tab_strip, tab_id, &tab_index))
    return false;

  result_.reset(ExtensionTabUtil::CreateTabValue(
      tab_strip->GetTabContentsAt(tab_index), tab_strip, tab_index));
  return true;
}

bool UpdateTabFunction::RunImpl() {
  int tab_id;
  EXTENSION_FUNCTION_VALIDATE(args_->IsType(Value::TYPE_DICTIONARY));
  const DictionaryValue *args = static_cast<const DictionaryValue*>(args_);
  EXTENSION_FUNCTION_VALIDATE(args->GetInteger(L"id", &tab_id));

  Browser* browser = BrowserList::GetLastActive();
  if (!browser)
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
      controller.LoadURL(new_gurl, GURL(), PageTransition::LINK);
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
  int tab_id;
  int new_index;
  EXTENSION_FUNCTION_VALIDATE(args_->IsType(Value::TYPE_DICTIONARY));
  const DictionaryValue *args = static_cast<const DictionaryValue*>(args_);
  EXTENSION_FUNCTION_VALIDATE(args->GetInteger(L"id", &tab_id));
  EXTENSION_FUNCTION_VALIDATE(args->GetInteger(L"index", &new_index));
  EXTENSION_FUNCTION_VALIDATE(new_index >= 0);

  Browser* browser = BrowserList::GetLastActive();
  if (!browser)
    return false;

  int tab_index;
  TabStripModel* tab_strip = browser->tabstrip_model();
  // TODO(rafaelw): return an error if the tab is not found by |tab_id|
  if (!GetIndexOfTabId(tab_strip, tab_id, &tab_index))
    return false;

  // TODO(rafaelw): support moving tabs between windows
  // -windowId

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
  int tab_id;
  EXTENSION_FUNCTION_VALIDATE(args_->GetAsInteger(&tab_id));

  Browser* browser = BrowserList::GetLastActive();
  if (!browser)
    return false;

  int tab_index;
  TabStripModel* tab_strip = browser->tabstrip_model();
  if (GetIndexOfTabId(tab_strip, tab_id, &tab_index)) {
    browser->CloseTabContents(tab_strip->GetTabContentsAt(tab_index));
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
    tab_list->Append(ExtensionTabUtil::CreateTabValue(
        tab_strip->GetTabContentsAt(i), tab_strip, i));
  }

  return tab_list;
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
