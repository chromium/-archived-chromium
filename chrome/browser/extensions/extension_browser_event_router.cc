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
#include "chrome/browser/extensions/extension_tabs_module.h"
#include "chrome/common/notification_service.h"

const char* kOnPageActionExecuted = "page-action-executed";
const char* kOnTabCreated = "tab-created";
const char* kOnTabMoved = "tab-moved";
const char* kOnTabSelectionChanged = "tab-selection-changed";
const char* kOnTabAttached = "tab-attached";
const char* kOnTabDetached = "tab-detached";
const char* kOnTabRemoved = "tab-removed";
const char* kOnWindowCreated = "window-created";
const char* kOnWindowRemoved = "window-removed";
const char* kOnWindowFocusedChanged = "window-focus-changed";

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
}

void ExtensionBrowserEventRouter::TabInsertedAt(TabContents* contents,
                                                int index,
                                                bool foreground) {
  // If tab is new, send tab-created event.
  int tab_id = ExtensionTabUtil::GetTabId(contents);
  if (tab_ids_.find(tab_id) == tab_ids_.end()) {
    tab_ids_.insert(tab_id);

    TabCreatedAt(contents, index, foreground);
    return;
  }

  ListValue args;
  args.Append(Value::CreateIntegerValue(tab_id));
  
  DictionaryValue *object_args = new DictionaryValue();
  object_args->Set(L"newWindowId", Value::CreateIntegerValue(
      ExtensionTabUtil::GetWindowIdOfTab(contents)));
  object_args->Set(L"newPosition", Value::CreateIntegerValue(index));
  args.Append(object_args);

  std::string json_args;
  JSONWriter::Write(&args, false, &json_args);

  DispatchEvent(contents->profile(), kOnTabAttached, json_args);
}

void ExtensionBrowserEventRouter::TabDetachedAt(TabContents* contents,
                                                int index) {
  int tab_id = ExtensionTabUtil::GetTabId(contents);
  if (tab_ids_.find(tab_id) == tab_ids_.end()) {
    // The tab was removed. Don't send detach event.
    return;
  }

  ListValue args;
  args.Append(Value::CreateIntegerValue(tab_id));
  
  DictionaryValue *object_args = new DictionaryValue();
  object_args->Set(L"oldWindowId", Value::CreateIntegerValue(
      ExtensionTabUtil::GetWindowIdOfTab(contents)));
  object_args->Set(L"oldPosition", Value::CreateIntegerValue(index));
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

  int removed_count = tab_ids_.erase(tab_id);
  DCHECK(removed_count > 0);
}

void ExtensionBrowserEventRouter::TabSelectedAt(TabContents* old_contents,
                                                TabContents* new_contents,
                                                int index,
                                                bool user_gesture) {
  ListValue args;
  args.Append(Value::CreateIntegerValue(
      ExtensionTabUtil::GetTabId(new_contents)));
  
  DictionaryValue *object_args = new DictionaryValue();
  object_args->Set(L"windowId", Value::CreateIntegerValue(
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
  object_args->Set(L"windowId", Value::CreateIntegerValue(
      ExtensionTabUtil::GetWindowIdOfTab(contents)));
  object_args->Set(L"fromIndex", Value::CreateIntegerValue(from_index));
  object_args->Set(L"toIndex", Value::CreateIntegerValue(to_index));
  args.Append(object_args);

  std::string json_args;
  JSONWriter::Write(&args, false, &json_args);

  DispatchEvent(contents->profile(), kOnTabMoved, json_args);
}

void ExtensionBrowserEventRouter::TabChangedAt(TabContents* contents,
                                               int index,
                                               bool loading_only) { }

void ExtensionBrowserEventRouter::TabStripEmpty() { }

void ExtensionBrowserEventRouter::PageActionExecuted(Profile *profile,
                                                     std::string page_action_id,
                                                     int tab_id,
                                                     std::string url) {
  ListValue args;
  DictionaryValue *object_args = new DictionaryValue();
  object_args->Set(L"pageActionId", Value::CreateStringValue(page_action_id));

  DictionaryValue *data = new DictionaryValue();
  data->Set(L"tabId", Value::CreateIntegerValue(tab_id));
  data->Set(L"tabUrl", Value::CreateStringValue(url));
  object_args->Set(L"data", data);

  args.Append(object_args);

  std::string json_args;
  JSONWriter::Write(&args, false, &json_args);

  DispatchEvent(profile, kOnPageActionExecuted, json_args);
}
