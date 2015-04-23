// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_tabs_module.h"

#include "base/string_util.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/browser_window.h"
#include "chrome/browser/extensions/extension_function_dispatcher.h"
#include "chrome/browser/extensions/extension_tabs_module_constants.h"
#include "chrome/browser/extensions/extensions_service.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/renderer_host/render_view_host_delegate.h"
#include "chrome/browser/tab_contents/navigation_entry.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/window_sizer.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_error_utils.h"


namespace keys = extension_tabs_module_constants;

// Forward declare static helper functions defined below.
static DictionaryValue* CreateWindowValue(Browser* browser, bool populate_tabs);
static ListValue* CreateTabList(Browser* browser);

// |error_message| can optionally be passed in a will be set with an appropriate
// message if the window cannot be found by id.
static Browser* GetBrowserInProfileWithId(Profile* profile,
                                          const int window_id,
                                          std::string* error_message);

// |error_message| can optionally be passed in a will be set with an appropriate
// message if the tab cannot be found by id.
static bool GetTabById(int tab_id, Profile* profile, Browser** browser,
                       TabStripModel** tab_strip,
                       TabContents** contents,
                       int* tab_index, std::string* error_message);

// Construct an absolute path from a relative path.
static GURL AbsolutePath(Profile* profile, std::string extension_id,
                         std::string relative_url);

int ExtensionTabUtil::GetWindowId(const Browser* browser) {
  return browser->session_id().id();
}

int ExtensionTabUtil::GetTabId(const TabContents* tab_contents) {
  return tab_contents->controller().session_id().id();
}

ExtensionTabUtil::TabStatus ExtensionTabUtil::GetTabStatus(
    const TabContents* tab_contents) {
  return tab_contents->is_loading() ? TAB_LOADING : TAB_COMPLETE;
}

std::string ExtensionTabUtil::GetTabStatusText(TabStatus status) {
  std::string text;
  switch (status) {
    case TAB_LOADING:
      text = keys::kStatusValueLoading;
      break;
    case TAB_COMPLETE:
      text = keys::kStatusValueComplete;
      break;
  }

  return text;
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
  TabStatus status = GetTabStatus(contents);

  DictionaryValue* result = new DictionaryValue();
  result->SetInteger(keys::kIdKey, ExtensionTabUtil::GetTabId(contents));
  result->SetInteger(keys::kIndexKey, tab_index);
  result->SetInteger(keys::kWindowIdKey,
                     ExtensionTabUtil::GetWindowIdOfTab(contents));
  result->SetString(keys::kUrlKey, contents->GetURL().spec());
  result->SetString(keys::kStatusKey, GetTabStatusText(status));
  result->SetBoolean(keys::kSelectedKey,
                     tab_strip && tab_index == tab_strip->selected_index());

  if (status != TAB_LOADING) {
    result->SetString(keys::kTitleKey, UTF16ToWide(contents->GetTitle()));

    NavigationEntry* entry = contents->controller().GetActiveEntry();
    if (entry) {
      if (entry->favicon().is_valid())
        result->SetString(keys::kFavIconUrlKey, entry->favicon().url().spec());
    }
  }

  return result;
}

bool ExtensionTabUtil::GetTabById(int tab_id, Profile* profile,
                                  Browser** browser,
                                  TabStripModel** tab_strip,
                                  TabContents** contents,
                                  int* tab_index) {
  Browser* target_browser;
  TabStripModel* target_tab_strip;
  TabContents* target_contents;
  for (BrowserList::const_iterator iter = BrowserList::begin();
       iter != BrowserList::end(); ++iter) {
    target_browser = *iter;
    if (target_browser->profile() == profile) {
      target_tab_strip = target_browser->tabstrip_model();
      for (int i = 0; i < target_tab_strip->count(); ++i) {
        target_contents = target_tab_strip->GetTabContentsAt(i);
        if (target_contents->controller().session_id().id() == tab_id) {
          if (browser)
            *browser = target_browser;
          if (tab_strip)
            *tab_strip = target_tab_strip;
          if (contents)
            *contents = target_contents;
          if (tab_index)
            *tab_index = i;
          return true;
        }
      }
    }
  }
  return false;
}

// Windows ---------------------------------------------------------------------

bool GetWindowFunction::RunImpl() {
  int window_id;
  EXTENSION_FUNCTION_VALIDATE(args_->GetAsInteger(&window_id));

  Browser* browser = GetBrowserInProfileWithId(profile(), window_id, &error_);
  if (!browser)
    return false;

  result_.reset(CreateWindowValue(browser, false));
  return true;
}

bool GetCurrentWindowFunction::RunImpl() {
  Browser* browser = dispatcher()->GetBrowser();
  if (!browser) {
    error_ = keys::kNoCurrentWindowError;
    return false;
  }
  result_.reset(CreateWindowValue(browser, false));
  return true;
}

bool GetLastFocusedWindowFunction::RunImpl() {
  Browser* browser = BrowserList::GetLastActiveWithProfile(profile());
  if (!browser) {
    error_ = keys::kNoLastFocusedWindowError;
    return false;
  }
  result_.reset(CreateWindowValue(browser, false));
  return true;
}

bool GetAllWindowsFunction::RunImpl() {
  bool populate_tabs = false;
  if (!args_->IsType(Value::TYPE_NULL)) {
    EXTENSION_FUNCTION_VALIDATE(args_->GetAsBoolean(&populate_tabs));
  }

  result_.reset(new ListValue());
  for (BrowserList::const_iterator browser = BrowserList::begin();
    browser != BrowserList::end(); ++browser) {
      // Only examine browsers in the current profile.
      if ((*browser)->profile() == profile()) {
        static_cast<ListValue*>(result_.get())->
          Append(CreateWindowValue(*browser, populate_tabs));
      }
  }

  return true;
}

bool CreateWindowFunction::RunImpl() {
  scoped_ptr<GURL> url(new GURL());

  // Look for optional url.
  if (!args_->IsType(Value::TYPE_NULL)) {
    EXTENSION_FUNCTION_VALIDATE(args_->IsType(Value::TYPE_DICTIONARY));
    const DictionaryValue *args = static_cast<const DictionaryValue*>(args_);
    std::string url_input;
    if (args->HasKey(keys::kUrlKey)) {
      EXTENSION_FUNCTION_VALIDATE(args->GetString(keys::kUrlKey,
                                                  &url_input));
      url.reset(new GURL(url_input));
      if (!url->is_valid()) {
        error_ = ExtensionErrorUtils::FormatErrorMessage(keys::kInvalidUrlError,
            url_input);
        return false;
      }
    }
  }

  // Try to position the new browser relative its originating browser window.
  gfx::Rect empty_bounds;
  gfx::Rect bounds;
  bool maximized;
  // The call offsets the bounds by kWindowTilePixels (defined in WindowSizer to
  // be 10)
  //
  // NOTE(rafaelw): It's ok if dispatcher_->GetBrowser() returns NULL here.
  // GetBrowserWindowBounds will default to saved "default" values for the app.
  WindowSizer::GetBrowserWindowBounds(std::wstring(), empty_bounds,
                                      dispatcher()->GetBrowser(), &bounds,
                                      &maximized);

  // Any part of the bounds can optionally be set by the caller.
  if (args_->IsType(Value::TYPE_DICTIONARY)) {
    const DictionaryValue *args = static_cast<const DictionaryValue*>(args_);
    int bounds_val;
    if (args->HasKey(keys::kLeftKey)) {
      EXTENSION_FUNCTION_VALIDATE(args->GetInteger(keys::kLeftKey,
                                                   &bounds_val));
      bounds.set_x(bounds_val);
    }

    if (args->HasKey(keys::kTopKey)) {
      EXTENSION_FUNCTION_VALIDATE(args->GetInteger(keys::kTopKey,
                                                   &bounds_val));
      bounds.set_y(bounds_val);
    }

    if (args->HasKey(keys::kWidthKey)) {
      EXTENSION_FUNCTION_VALIDATE(args->GetInteger(keys::kWidthKey,
                                                   &bounds_val));
      bounds.set_width(bounds_val);
    }

    if (args->HasKey(keys::kHeightKey)) {
      EXTENSION_FUNCTION_VALIDATE(args->GetInteger(keys::kHeightKey,
                                                   &bounds_val));
      bounds.set_height(bounds_val);
    }
  }

  Browser* new_window = Browser::Create(dispatcher()->profile());
  new_window->AddTabWithURL(*(url.get()), GURL(), PageTransition::LINK, true,
                            -1, false, NULL);

  new_window->window()->SetBounds(bounds);
  new_window->window()->Show();

  // TODO(rafaelw): support |focused|, |zIndex|

  result_.reset(CreateWindowValue(new_window, false));

  return true;
}

bool UpdateWindowFunction::RunImpl() {
  EXTENSION_FUNCTION_VALIDATE(args_->IsType(Value::TYPE_LIST));
  const ListValue* args = static_cast<const ListValue*>(args_);
  int window_id;
  EXTENSION_FUNCTION_VALIDATE(args->GetInteger(0, &window_id));
  DictionaryValue* update_props;
  EXTENSION_FUNCTION_VALIDATE(args->GetDictionary(1, &update_props));

  Browser* browser = GetBrowserInProfileWithId(profile(), window_id, &error_);
  if (!browser)
    return false;

  gfx::Rect bounds = browser->window()->GetNormalBounds();
  // Any part of the bounds can optionally be set by the caller.
  int bounds_val;
  if (update_props->HasKey(keys::kLeftKey)) {
    EXTENSION_FUNCTION_VALIDATE(update_props->GetInteger(
        keys::kLeftKey,
        &bounds_val));
    bounds.set_x(bounds_val);
  }

  if (update_props->HasKey(keys::kTopKey)) {
    EXTENSION_FUNCTION_VALIDATE(update_props->GetInteger(
        keys::kTopKey,
        &bounds_val));
    bounds.set_y(bounds_val);
  }

  if (update_props->HasKey(keys::kWidthKey)) {
    EXTENSION_FUNCTION_VALIDATE(update_props->GetInteger(
        keys::kWidthKey,
        &bounds_val));
    bounds.set_width(bounds_val);
  }

  if (update_props->HasKey(keys::kHeightKey)) {
    EXTENSION_FUNCTION_VALIDATE(update_props->GetInteger(
        keys::kHeightKey,
        &bounds_val));
    bounds.set_height(bounds_val);
  }

  browser->window()->SetBounds(bounds);
  // TODO(rafaelw): Support |focused|.
  result_.reset(CreateWindowValue(browser, false));

  return true;
}

bool RemoveWindowFunction::RunImpl() {
  int window_id;
  EXTENSION_FUNCTION_VALIDATE(args_->GetAsInteger(&window_id));

  Browser* browser = GetBrowserInProfileWithId(profile(), window_id, &error_);
  if (!browser)
    return false;

  browser->CloseWindow();

  return true;
}

// Tabs ------------------------------------------------------------------------

bool GetSelectedTabFunction::RunImpl() {
  Browser* browser;
  // windowId defaults to "current" window.
  int window_id = -1;

  if (!args_->IsType(Value::TYPE_NULL)) {
    EXTENSION_FUNCTION_VALIDATE(args_->GetAsInteger(&window_id));
    browser = GetBrowserInProfileWithId(profile(), window_id, &error_);
  } else {
    browser = dispatcher()->GetBrowser();
    if (!browser)
      error_ = keys::kNoCurrentWindowError;
  }
  if (!browser)
    return false;

  TabStripModel* tab_strip = browser->tabstrip_model();
  TabContents* contents = tab_strip->GetSelectedTabContents();
  if (!contents) {
    error_ = keys::kNoSelectedTabError;
    return false;
  }
  result_.reset(ExtensionTabUtil::CreateTabValue(contents, tab_strip,
      tab_strip->selected_index()));
  return true;
}

bool GetAllTabsInWindowFunction::RunImpl() {
  Browser* browser;
  // windowId defaults to "current" window.
  int window_id = -1;
  if (!args_->IsType(Value::TYPE_NULL)) {
    EXTENSION_FUNCTION_VALIDATE(args_->GetAsInteger(&window_id));
    browser = GetBrowserInProfileWithId(profile(), window_id, &error_);
  } else {
    browser = dispatcher()->GetBrowser();
    if (!browser)
      error_ = keys::kNoCurrentWindowError;
  }
  if (!browser)
    return false;

  result_.reset(CreateTabList(browser));

  return true;
}

bool CreateTabFunction::RunImpl() {
  EXTENSION_FUNCTION_VALIDATE(args_->IsType(Value::TYPE_DICTIONARY));
  const DictionaryValue* args = static_cast<const DictionaryValue*>(args_);

  Browser *browser;
  // windowId defaults to "current" window.
  int window_id = -1;
  if (args->HasKey(keys::kWindowIdKey)) {
    EXTENSION_FUNCTION_VALIDATE(args->GetInteger(
        keys::kWindowIdKey, &window_id));
    browser = GetBrowserInProfileWithId(profile(), window_id, &error_);
  } else {
    browser = dispatcher()->GetBrowser();
    if (!browser)
      error_ = keys::kNoCurrentWindowError;
  }
  if (!browser)
    return false;

  TabStripModel* tab_strip = browser->tabstrip_model();

  // TODO(rafaelw): handle setting remaining tab properties:
  // -title
  // -favIconUrl

  std::string url_string;
  scoped_ptr<GURL> url(new GURL());
  if (args->HasKey(keys::kUrlKey)) {
    EXTENSION_FUNCTION_VALIDATE(args->GetString(keys::kUrlKey,
                                                &url_string));
    url.reset(new GURL(url_string));
    if (!url->is_valid()) {
      // The path as passed in is not valid. Try converting to absolute path.
      *url = AbsolutePath(profile(), extension_id(), url_string);
      if (!url->is_valid()) {
        error_ = ExtensionErrorUtils::FormatErrorMessage(keys::kInvalidUrlError,
                                                         url_string);
        return false;
      }
    }
  }

  // Default to foreground for the new tab. The presence of 'selected' property
  // will override this default.
  bool selected = true;
  if (args->HasKey(keys::kSelectedKey))
    EXTENSION_FUNCTION_VALIDATE(args->GetBoolean(keys::kSelectedKey,
                                                 &selected));
  // If index is specified, honor the value, but keep it bound to
  // 0 <= index <= tab_strip->count()
  int index = -1;
  if (args->HasKey(keys::kIndexKey))
    EXTENSION_FUNCTION_VALIDATE(args->GetInteger(keys::kIndexKey,
                                                 &index));

  if (index < 0) {
    // Default insert behavior.
    index = -1;
  }
  if (index > tab_strip->count()) {
    index = tab_strip->count();
  }

  TabContents* contents = browser->AddTabWithURL(*(url.get()), GURL(),
      PageTransition::LINK, selected, index, true, NULL);
  index = tab_strip->GetIndexOfTabContents(contents);

  // Return data about the newly created tab.
  if (has_callback())
    result_.reset(ExtensionTabUtil::CreateTabValue(contents, tab_strip, index));

  return true;
}

bool GetTabFunction::RunImpl() {
  int tab_id;
  EXTENSION_FUNCTION_VALIDATE(args_->GetAsInteger(&tab_id));

  TabStripModel* tab_strip = NULL;
  TabContents* contents = NULL;
  int tab_index = -1;
  if (!GetTabById(tab_id, profile(), NULL, &tab_strip, &contents, &tab_index,
      &error_))
    return false;

  result_.reset(ExtensionTabUtil::CreateTabValue(contents, tab_strip,
      tab_index));
  return true;
}

bool UpdateTabFunction::RunImpl() {
  int tab_id;
  EXTENSION_FUNCTION_VALIDATE(args_->IsType(Value::TYPE_LIST));
  const ListValue* args = static_cast<const ListValue*>(args_);
  EXTENSION_FUNCTION_VALIDATE(args->GetInteger(0, &tab_id));
  DictionaryValue* update_props;
  EXTENSION_FUNCTION_VALIDATE(args->GetDictionary(1, &update_props));

  TabStripModel* tab_strip = NULL;
  TabContents* contents = NULL;
  int tab_index = -1;
  if (!GetTabById(tab_id, profile(), NULL, &tab_strip, &contents, &tab_index,
      &error_))
    return false;

  NavigationController& controller = contents->controller();

  // TODO(rafaelw): handle setting remaining tab properties:
  // -title
  // -favIconUrl

  // Navigate the tab to a new location if the url different.
  std::string url;
  if (update_props->HasKey(keys::kUrlKey)) {
    EXTENSION_FUNCTION_VALIDATE(update_props->GetString(
        keys::kUrlKey, &url));
    GURL new_gurl(url);

    if (!new_gurl.is_valid()) {
      // The path as passed in is not valid. Try converting to absolute path.
      new_gurl = AbsolutePath(profile(), extension_id(), url);
      if (!new_gurl.is_valid()) {
        error_ = ExtensionErrorUtils::FormatErrorMessage(keys::kInvalidUrlError,
                                                         url);
        return false;
      }
    }

    controller.LoadURL(new_gurl, GURL(), PageTransition::LINK);
  }

  bool selected = false;
  // TODO(rafaelw): Setting |selected| from js doesn't make much sense.
  // Move tab selection management up to window.
  if (update_props->HasKey(keys::kSelectedKey)) {
    EXTENSION_FUNCTION_VALIDATE(update_props->GetBoolean(
        keys::kSelectedKey,
        &selected));
    if (selected && tab_strip->selected_index() != tab_index) {
      tab_strip->SelectTabContentsAt(tab_index, false);
    }
  }

  return true;
}

bool MoveTabFunction::RunImpl() {
  int tab_id;
  EXTENSION_FUNCTION_VALIDATE(args_->IsType(Value::TYPE_LIST));
  const ListValue* args = static_cast<const ListValue*>(args_);
  EXTENSION_FUNCTION_VALIDATE(args->GetInteger(0, &tab_id));
  DictionaryValue* update_props;
  EXTENSION_FUNCTION_VALIDATE(args->GetDictionary(1, &update_props));

  int new_index;
  EXTENSION_FUNCTION_VALIDATE(update_props->GetInteger(
      keys::kIndexKey, &new_index));
  EXTENSION_FUNCTION_VALIDATE(new_index >= 0);

  Browser* source_browser = NULL;
  TabStripModel* source_tab_strip = NULL;
  int tab_index = -1;
  if (!GetTabById(tab_id, profile(), &source_browser, &source_tab_strip, NULL,
      &tab_index, &error_))
    return false;

  if (update_props->HasKey(keys::kWindowIdKey)) {
    Browser* target_browser;
    int window_id;
    EXTENSION_FUNCTION_VALIDATE(update_props->GetInteger(
        keys::kWindowIdKey, &window_id));
    target_browser = GetBrowserInProfileWithId(profile(), window_id,
        &error_);
    if (!target_browser)
      return false;

    // If windowId is different from the current window, move between windows.
    if (ExtensionTabUtil::GetWindowId(target_browser) !=
        ExtensionTabUtil::GetWindowId(source_browser)) {
      TabStripModel* target_tab_strip = target_browser->tabstrip_model();
      TabContents *contents = source_tab_strip->DetachTabContentsAt(tab_index);
      if (!contents) {
        error_ = ExtensionErrorUtils::FormatErrorMessage(
            keys::kTabNotFoundError, IntToString(tab_id));
        return false;
      }

      // Clamp move location to the last position.
      // This is ">" because it can append to a new index position.
      if (new_index > target_tab_strip->count())
        new_index = target_tab_strip->count();

      target_tab_strip->InsertTabContentsAt(new_index, contents,
          false, true);

      return true;
    }
  }

  // Perform a simple within-window move.
  // Clamp move location to the last position.
  // This is ">=" because the move must be to an existing location.
  if (new_index >= source_tab_strip->count())
    new_index = source_tab_strip->count() - 1;

  if (new_index != tab_index)
    source_tab_strip->MoveTabContentsAt(tab_index, new_index, false);

  return true;
}


bool RemoveTabFunction::RunImpl() {
  int tab_id;
  EXTENSION_FUNCTION_VALIDATE(args_->GetAsInteger(&tab_id));

  Browser* browser = NULL;
  TabContents* contents = NULL;
  if (!GetTabById(tab_id, profile(), &browser, NULL, &contents, NULL, &error_))
    return false;

  browser->CloseTabContents(contents);
  return true;
}

bool GetTabLanguageFunction::RunImpl() {
  int tab_id = 0;
  Browser* browser = NULL;
  TabContents* contents = NULL;

  // If |tab_id| is specified, look for it. Otherwise default to selected tab
  // in the current window.
  if (!args_->IsType(Value::TYPE_NULL)) {
    EXTENSION_FUNCTION_VALIDATE(args_->GetAsInteger(&tab_id));
    if (!GetTabById(tab_id, profile(), &browser, NULL, &contents, NULL,
        &error_)) {
      return false;
    }
    if (!browser || !contents)
      return false;
  } else {
    browser = dispatcher()->GetBrowser();
    if (!browser)
      return false;
    contents = browser->tabstrip_model()->GetSelectedTabContents();
    if (!contents)
      return false;
  }

  // Figure out what language |contents| contains. This sends an async call via
  // the browser to the renderer to determine the language of the tab the
  // renderer has. The renderer sends back the language of the tab after the
  // tab loads (it may be delayed) to the browser, which in turn notifies this
  // object that the language has been received.
  contents->GetPageLanguage();
  registrar_.Add(this, NotificationType::TAB_LANGUAGE_DETERMINED,
      NotificationService::AllSources());
  AddRef();  // balanced in Observe()
  return true;
}

void GetTabLanguageFunction::Observe(NotificationType type,
                                     const NotificationSource& source,
                                     const NotificationDetails& details) {
  DCHECK(type == NotificationType::TAB_LANGUAGE_DETERMINED);
  std::string language(*Details<std::string>(details).ptr());
  result_.reset(Value::CreateStringValue(language.c_str()));
  SendResponse(true);
  Release();  // balanced in Run()
}

// static helpers

// if |populate| is true, each window gets a list property |tabs| which contains
// fully populated tab objects.
static DictionaryValue* CreateWindowValue(Browser* browser,
                                          bool populate_tabs) {
  DictionaryValue* result = new DictionaryValue();
  result->SetInteger(keys::kIdKey, ExtensionTabUtil::GetWindowId(
      browser));
  result->SetBoolean(keys::kFocusedKey,
      browser->window()->IsActive());
  gfx::Rect bounds = browser->window()->GetNormalBounds();

  // TODO(rafaelw): zIndex ?
  result->SetInteger(keys::kLeftKey, bounds.x());
  result->SetInteger(keys::kTopKey, bounds.y());
  result->SetInteger(keys::kWidthKey, bounds.width());
  result->SetInteger(keys::kHeightKey, bounds.height());

  if (populate_tabs) {
    result->Set(keys::kTabsKey, CreateTabList(browser));
  }

  return result;
}

static ListValue* CreateTabList(Browser* browser) {
  ListValue* tab_list = new ListValue();
  TabStripModel* tab_strip = browser->tabstrip_model();
  for (int i = 0; i < tab_strip->count(); ++i) {
    tab_list->Append(ExtensionTabUtil::CreateTabValue(
        tab_strip->GetTabContentsAt(i), tab_strip, i));
  }

  return tab_list;
}

static Browser* GetBrowserInProfileWithId(Profile* profile,
                                          const int window_id,
                                          std::string* error_message) {
  for (BrowserList::const_iterator browser = BrowserList::begin();
      browser != BrowserList::end(); ++browser) {
    if ((*browser)->profile() == profile &&
        ExtensionTabUtil::GetWindowId(*browser) == window_id)
      return *browser;
  }

  if (error_message)
    *error_message= ExtensionErrorUtils::FormatErrorMessage(
        keys::kWindowNotFoundError, IntToString(window_id));

  return NULL;
}

static GURL AbsolutePath(Profile* profile, std::string extension_id,
                         std::string relative_url) {
  ExtensionsService* service = profile->GetExtensionsService();
  Extension* extension = service->GetExtensionById(extension_id);
  return Extension::GetResourceURL(extension->url(), relative_url);
}

static bool GetTabById(int tab_id, Profile* profile, Browser** browser,
                       TabStripModel** tab_strip,
                       TabContents** contents,
                       int* tab_index,
                       std::string* error_message) {
  if (ExtensionTabUtil::GetTabById(tab_id, profile, browser, tab_strip,
                                   contents, tab_index))
    return true;

  if (error_message)
    *error_message = ExtensionErrorUtils::FormatErrorMessage(
        keys::kTabNotFoundError, IntToString(tab_id));

  return false;
}
