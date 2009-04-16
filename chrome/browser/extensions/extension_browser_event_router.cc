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

const char* kOnTabMoved = "tab-moved";

ExtensionBrowserEventRouter* ExtensionBrowserEventRouter::GetInstance() {
  return Singleton<ExtensionBrowserEventRouter>::get();
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
      break;
    case(NotificationType::BROWSER_CLOSED) :
      browser = Source<Browser>(source).ptr();
      browser->tabstrip_model()->RemoveObserver(this);
      break;
    default:
      NOTREACHED();
      break;
  }
}

void ExtensionBrowserEventRouter::TabInsertedAt(TabContents* contents,
                                                int index,
                                                bool foreground) { }

void ExtensionBrowserEventRouter::TabClosingAt(TabContents* contents,
                                               int index) { }

void ExtensionBrowserEventRouter::TabDetachedAt(TabContents* contents,
                                                int index) { }

void ExtensionBrowserEventRouter::TabSelectedAt(TabContents* old_contents,
                                                TabContents* new_contents,
                                                int index,
                                                bool user_gesture) { }

void ExtensionBrowserEventRouter::TabMoved(TabContents* contents,
                                           int from_index,
                                           int to_index) {
  Profile *profile = contents->profile();

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

  ExtensionMessageService::GetInstance(profile->GetRequestContext())->
      DispatchEventToRenderers(kOnTabMoved, json_args);
}

void ExtensionBrowserEventRouter::TabChangedAt(TabContents* contents,
                                               int index,
                                               bool loading_only) { }

void ExtensionBrowserEventRouter::TabStripEmpty() { }
