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

const char* kOnWindowCreated = "window-created";
const char* kOnWindowRemoved = "window-removed";
const char* kOnTabCreated = "tab-created";
const char* kOnTabMoved = "tab-moved";
const char* kOnTabSelectionChanged = "tab-selection-changed";
const char* kOnTabAttached = "tab-attached";
const char* kOnTabDetached = "tab-detached";
const char* kOnTabRemoved = "tab-removed";

ExtensionBrowserEventRouter* ExtensionBrowserEventRouter::GetInstance() {
  return Singleton<ExtensionBrowserEventRouter>::get();
}

static void DispatchEvent(Profile *profile,
                          const char* event_name,
                          const std::string json_args) {
  ExtensionMessageService::GetInstance(profile->GetRequestContext())->
      DispatchEventToRenderers(event_name, json_args);
}

void ExtensionBrowserEventRouter::Init() {
  if (initialized_)
    return;

  NotificationService::current()->AddObserver(this,
    NotificationType::BROWSER_OPENED, NotificationService::AllSources());
  NotificationService::current()->AddObserver(this,
    NotificationType::BROWSER_CLOSED, NotificationService::AllSources());

  initialized_ = true;
}

ExtensionBrowserEventRouter::ExtensionBrowserEventRouter()
    : initialized_(false) { }

// NotificationObserver
void ExtensionBrowserEventRouter::Observe(NotificationType type,
    const NotificationSource& source, const NotificationDetails& details) {
  Browser* browser;

  switch (type.value) {
    case(NotificationType::BROWSER_OPENED) :
      browser = Source<Browser>(source).ptr();
      browser->tabstrip_model()->AddObserver(this);
      BrowserOpened(browser);
      break;
    case(NotificationType::BROWSER_CLOSED) :
      browser = Source<Browser>(source).ptr();
      browser->tabstrip_model()->RemoveObserver(this);
      BrowserClosed(browser);
      break;
    default:
      NOTREACHED();
      break;
  }
}

void ExtensionBrowserEventRouter::BrowserOpened(Browser* browser) {
  int window_id = ExtensionTabUtil::GetWindowId(browser);

  ListValue args;
  args.Append(Value::CreateIntegerValue(window_id));

  std::string json_args;
  JSONWriter::Write(&args, false, &json_args);

  DispatchEvent(browser->profile(), kOnWindowCreated, json_args);
}

void ExtensionBrowserEventRouter::BrowserClosed(Browser* browser) {
  int window_id = ExtensionTabUtil::GetWindowId(browser);

  ListValue args;
  args.Append(Value::CreateIntegerValue(window_id));

  std::string json_args;
  JSONWriter::Write(&args, false, &json_args);

  DispatchEvent(browser->profile(), kOnWindowRemoved, json_args);
}

void ExtensionBrowserEventRouter::TabInsertedAt(TabContents* contents,
                                                int index,
                                                bool foreground) {
  const char* event_name = kOnTabAttached;
  // If tab is new, send tab-created event.
  int tab_id = ExtensionTabUtil::GetTabId(contents);
  if (tab_ids_.find(tab_id) == tab_ids_.end()) {
    tab_ids_.insert(tab_id);
    event_name = kOnTabCreated;
  }

  ListValue args;
  DictionaryValue *object_args = new DictionaryValue();
  object_args->Set(L"tabId", Value::CreateIntegerValue(tab_id));
  object_args->Set(L"windowId", Value::CreateIntegerValue(
      ExtensionTabUtil::GetWindowIdOfTab(contents)));
  object_args->Set(L"index", Value::CreateIntegerValue(index));
  args.Append(object_args);

  std::string json_args;
  JSONWriter::Write(&args, false, &json_args);

  DispatchEvent(contents->profile(), event_name, json_args);
}

void ExtensionBrowserEventRouter::TabDetachedAt(TabContents* contents,
                                                int index) {
  int tab_id = ExtensionTabUtil::GetTabId(contents);
  if (tab_ids_.find(tab_id) == tab_ids_.end()) {
    // The tab was removed. Don't send detach event.
    return;
  }

  ListValue args;
  DictionaryValue *object_args = new DictionaryValue();
  object_args->Set(L"tabId", Value::CreateIntegerValue(tab_id));
  object_args->Set(L"windowId", Value::CreateIntegerValue(
      ExtensionTabUtil::GetWindowIdOfTab(contents)));
  object_args->Set(L"index", Value::CreateIntegerValue(index));
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
  DictionaryValue *object_args = new DictionaryValue();
  object_args->Set(L"tabId", Value::CreateIntegerValue(
      ExtensionTabUtil::GetTabId(new_contents)));
  object_args->Set(L"windowId", Value::CreateIntegerValue(
      ExtensionTabUtil::GetWindowIdOfTab(new_contents)));
  object_args->Set(L"index", Value::CreateIntegerValue(index));
  args.Append(object_args);

  std::string json_args;
  JSONWriter::Write(&args, false, &json_args);

  DispatchEvent(new_contents->profile(), kOnTabSelectionChanged, json_args);
}

void ExtensionBrowserEventRouter::TabMoved(TabContents* contents,
                                           int from_index,
                                           int to_index) {
  ListValue args;
  DictionaryValue *object_args = new DictionaryValue();
  object_args->Set(L"tabId", Value::CreateIntegerValue(
      ExtensionTabUtil::GetTabId(contents)));
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
