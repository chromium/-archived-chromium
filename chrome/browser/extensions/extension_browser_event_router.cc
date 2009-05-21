// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_browser_event_router.h"

#include "base/json_writer.h"
#include "base/values.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/extensions/extension.h"
#include "chrome/browser/extensions/extension_message_service.h"
#include "chrome/browser/tab_contents/navigation_entry.h"
#include "chrome/common/notification_service.h"

const char* kOnPageActionExecuted = "page-action-executed";
const char* kOnTabAttached = "tab-attached";
const char* kOnTabCreated = "tab-created";
const char* kOnTabDetached = "tab-detached";
const char* kOnTabMoved = "tab-moved";
const char* kOnTabRemoved = "tab-removed";
const char* kOnTabSelectionChanged = "tab-selection-changed";
const char* kOnTabUpdated = "tab-updated";
const char* kOnWindowCreated = "window-created";
const char* kOnWindowFocusedChanged = "window-focus-changed";
const char* kOnWindowRemoved = "window-removed";

ExtensionBrowserEventRouter::TabEntry::TabEntry()
    : state_(ExtensionTabUtil::TAB_COMPLETE),
      pending_navigate_(false),
      url_() {
}

ExtensionBrowserEventRouter::TabEntry::TabEntry(const TabContents* contents)
    : state_(ExtensionTabUtil::TAB_COMPLETE),
      pending_navigate_(false),
      url_(contents->GetURL()) {
  UpdateLoadState(contents);
}

DictionaryValue* ExtensionBrowserEventRouter::TabEntry::UpdateLoadState(
    const TabContents* contents) {
  ExtensionTabUtil::TabStatus old_state = state_;
  state_ = ExtensionTabUtil::GetTabStatus(contents);

  if (old_state == state_)
    return false;

  if (state_ == ExtensionTabUtil::TAB_LOADING) {
    // Do not send "loading" state changed now. Wait for navigate so the new
    // url is available.
    pending_navigate_ = true;
    return NULL;

  } else if (state_ == ExtensionTabUtil::TAB_COMPLETE) {
    // Send "complete" state change.
    DictionaryValue* changed_properties = new DictionaryValue();
    changed_properties->SetString(ExtensionTabUtil::kStatusKey,
        ExtensionTabUtil::kStatusValueComplete);
    return changed_properties;

  } else {
    NOTREACHED();
    return NULL;
  }
}

DictionaryValue* ExtensionBrowserEventRouter::TabEntry::DidNavigate(
    const TabContents* contents) {
  if(!pending_navigate_)
    return NULL;

  DictionaryValue* changed_properties = new DictionaryValue();
  changed_properties->SetString(ExtensionTabUtil::kStatusKey,
      ExtensionTabUtil::kStatusValueLoading);

  GURL new_url = contents->GetURL();
  if (new_url != url_) {
    url_ = new_url;
    changed_properties->SetString(ExtensionTabUtil::kUrlKey, url_.spec());
  }

  pending_navigate_ = false;
  return changed_properties;
}

ExtensionBrowserEventRouter* ExtensionBrowserEventRouter::GetInstance() {
  return Singleton<ExtensionBrowserEventRouter>::get();
}

static void DispatchEvent(Profile *profile,
                          const char* event_name,
                          const std::string json_args) {
  ExtensionMessageService::GetInstance(profile->GetRequestContext())->
      DispatchEventToRenderers(event_name, json_args);
}

static void DispatchSimpleBrowserEvent(Profile *profile, const int window_id,
                                       const char* event_name) {
  ListValue args;
  args.Append(Value::CreateIntegerValue(window_id));

  std::string json_args;
  JSONWriter::Write(&args, false, &json_args);

  DispatchEvent(profile, event_name, json_args);
}

void ExtensionBrowserEventRouter::Init() {
  if (initialized_)
    return;

  BrowserList::AddObserver(this);

  initialized_ = true;
}

ExtensionBrowserEventRouter::ExtensionBrowserEventRouter()
    : initialized_(false) { }

void ExtensionBrowserEventRouter::OnBrowserAdded(const Browser* browser) {
  // Start listening to TabStripModel events for this browser.
  browser->tabstrip_model()->AddObserver(this);

  DispatchSimpleBrowserEvent(browser->profile(),
                             ExtensionTabUtil::GetWindowId(browser),
                             kOnWindowCreated);
}

void ExtensionBrowserEventRouter::OnBrowserRemoving(const Browser* browser) {
  // Stop listening to TabStripModel events for this browser.
  browser->tabstrip_model()->RemoveObserver(this);

  DispatchSimpleBrowserEvent(browser->profile(),
                             ExtensionTabUtil::GetWindowId(browser),
                             kOnWindowRemoved);
}

void ExtensionBrowserEventRouter::OnBrowserSetLastActive(
    const Browser* browser) {
  DispatchSimpleBrowserEvent(browser->profile(),
                             ExtensionTabUtil::GetWindowId(browser),
                             kOnWindowFocusedChanged);
}

void ExtensionBrowserEventRouter::TabCreatedAt(TabContents* contents,
                                                int index,
                                                bool foreground) {
  ListValue args;
  args.Append(ExtensionTabUtil::CreateTabValue(contents));
  std::string json_args;
  JSONWriter::Write(&args, false, &json_args);

  DispatchEvent(contents->profile(), kOnTabCreated, json_args);

  registrar_.Add(this, NotificationType::NAV_ENTRY_COMMITTED,
                 Source<NavigationController>(&contents->controller()));
}

void ExtensionBrowserEventRouter::TabInsertedAt(TabContents* contents,
                                                int index,
                                                bool foreground) {
  // If tab is new, send tab-created event.
  int tab_id = ExtensionTabUtil::GetTabId(contents);
  if (tab_entries_.find(tab_id) == tab_entries_.end()) {
    tab_entries_[tab_id] = TabEntry(contents);

    TabCreatedAt(contents, index, foreground);
    return;
  }

  ListValue args;
  args.Append(Value::CreateIntegerValue(tab_id));

  DictionaryValue *object_args = new DictionaryValue();
  object_args->Set(ExtensionTabUtil::kNewWindowIdKey, Value::CreateIntegerValue(
      ExtensionTabUtil::GetWindowIdOfTab(contents)));
  object_args->Set(ExtensionTabUtil::kNewPositionKey, Value::CreateIntegerValue(
      index));
  args.Append(object_args);

  std::string json_args;
  JSONWriter::Write(&args, false, &json_args);

  DispatchEvent(contents->profile(), kOnTabAttached, json_args);
}

void ExtensionBrowserEventRouter::TabDetachedAt(TabContents* contents,
                                                int index) {
  int tab_id = ExtensionTabUtil::GetTabId(contents);
  if (tab_entries_.find(tab_id) == tab_entries_.end()) {
    // The tab was removed. Don't send detach event.
    return;
  }

  ListValue args;
  args.Append(Value::CreateIntegerValue(tab_id));

  DictionaryValue *object_args = new DictionaryValue();
  object_args->Set(ExtensionTabUtil::kOldWindowIdKey, Value::CreateIntegerValue(
      ExtensionTabUtil::GetWindowIdOfTab(contents)));
  object_args->Set(ExtensionTabUtil::kOldPositionKey, Value::CreateIntegerValue(
      index));
  args.Append(object_args);

  std::string json_args;
  JSONWriter::Write(&args, false, &json_args);

  DispatchEvent(contents->profile(), kOnTabDetached, json_args);
}

void ExtensionBrowserEventRouter::TabClosingAt(TabContents* contents,
                                               int index) {
  int tab_id = ExtensionTabUtil::GetTabId(contents);

  ListValue args;
  args.Append(Value::CreateIntegerValue(tab_id));

  std::string json_args;
  JSONWriter::Write(&args, false, &json_args);

  DispatchEvent(contents->profile(), kOnTabRemoved, json_args);

  int removed_count = tab_entries_.erase(tab_id);
  DCHECK(removed_count > 0);

  registrar_.Remove(this, NotificationType::NAV_ENTRY_COMMITTED,
                    Source<NavigationController>(&contents->controller()));
}

void ExtensionBrowserEventRouter::TabSelectedAt(TabContents* old_contents,
                                                TabContents* new_contents,
                                                int index,
                                                bool user_gesture) {
  ListValue args;
  args.Append(Value::CreateIntegerValue(
      ExtensionTabUtil::GetTabId(new_contents)));

  DictionaryValue *object_args = new DictionaryValue();
  object_args->Set(ExtensionTabUtil::kWindowIdKey, Value::CreateIntegerValue(
      ExtensionTabUtil::GetWindowIdOfTab(new_contents)));
  args.Append(object_args);

  std::string json_args;
  JSONWriter::Write(&args, false, &json_args);

  DispatchEvent(new_contents->profile(), kOnTabSelectionChanged, json_args);
}

void ExtensionBrowserEventRouter::TabMoved(TabContents* contents,
                                           int from_index,
                                           int to_index) {
  ListValue args;
  args.Append(Value::CreateIntegerValue(ExtensionTabUtil::GetTabId(contents)));

  DictionaryValue *object_args = new DictionaryValue();
  object_args->Set(ExtensionTabUtil::kWindowIdKey, Value::CreateIntegerValue(
      ExtensionTabUtil::GetWindowIdOfTab(contents)));
  object_args->Set(ExtensionTabUtil::kFromIndexKey, Value::CreateIntegerValue(
      from_index));
  object_args->Set(ExtensionTabUtil::kToIndexKey, Value::CreateIntegerValue(
      to_index));
  args.Append(object_args);

  std::string json_args;
  JSONWriter::Write(&args, false, &json_args);

  DispatchEvent(contents->profile(), kOnTabMoved, json_args);
}

void ExtensionBrowserEventRouter::TabUpdated(TabContents* contents,
                                             bool did_navigate) {
  int tab_id = ExtensionTabUtil::GetTabId(contents);
  std::map<int, TabEntry>::iterator i = tab_entries_.find(tab_id);
  CHECK(tab_entries_.end() != i);
  TabEntry& entry = i->second;

  DictionaryValue* changed_properties = NULL;
  if (did_navigate)
    changed_properties = entry.DidNavigate(contents);
  else
    changed_properties = entry.UpdateLoadState(contents);

  if (changed_properties) {
    // The state of the tab (as seen from the extension point of view) has
    // changed.  Send a notification to the extension.
    ListValue args;
    args.Append(Value::CreateIntegerValue(tab_id));
    args.Append(changed_properties);

    std::string json_args;
    JSONWriter::Write(&args, false, &json_args);

    DispatchEvent(contents->profile(), kOnTabUpdated, json_args);
  }
}

void ExtensionBrowserEventRouter::Observe(NotificationType type,
                                          const NotificationSource& source,
                                          const NotificationDetails& details) {
  if (type == NotificationType::NAV_ENTRY_COMMITTED) {
    NavigationController* source_controller =
        Source<NavigationController>(source).ptr();
    TabUpdated(source_controller->tab_contents(), true);
  } else {
    NOTREACHED();
  }
}

void ExtensionBrowserEventRouter::TabChangedAt(TabContents* contents,
                                               int index,
                                               bool loading_only) {
  TabUpdated(contents, false);
}

void ExtensionBrowserEventRouter::TabStripEmpty() { }

void ExtensionBrowserEventRouter::PageActionExecuted(Profile *profile,
                                                     std::string page_action_id,
                                                     int tab_id,
                                                     std::string url) {
  ListValue args;
  DictionaryValue *object_args = new DictionaryValue();
  object_args->Set(ExtensionTabUtil::kPageActionIdKey,
                   Value::CreateStringValue(page_action_id));
  DictionaryValue *data = new DictionaryValue();
  data->Set(ExtensionTabUtil::kTabIdKey, Value::CreateIntegerValue(tab_id));
  data->Set(ExtensionTabUtil::kTabUrlKey, Value::CreateStringValue(url));
  object_args->Set(ExtensionTabUtil::kDataKey, data);

  args.Append(object_args);

  std::string json_args;
  JSONWriter::Write(&args, false, &json_args);

  DispatchEvent(profile, kOnPageActionExecuted, json_args);
}
