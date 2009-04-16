// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_process_manager.h"

#include "base/json_writer.h"
#include "base/singleton.h"
#include "base/values.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/extensions/extension_view.h"
#include "chrome/browser/extensions/extension_message_service.h"
#include "chrome/browser/tab_contents/site_instance.h"
#include "chrome/common/render_messages.h"

// static
ExtensionProcessManager* ExtensionProcessManager::GetInstance() {
  return Singleton<ExtensionProcessManager>::get();
}

ExtensionProcessManager::ExtensionProcessManager() {
}

ExtensionProcessManager::~ExtensionProcessManager() {
}

ExtensionView* ExtensionProcessManager::CreateView(Extension* extension,
                                                   const GURL& url,
                                                   Browser* browser) {
  return new ExtensionView(extension, url,
                           GetSiteInstanceForURL(url, browser->profile()),
                           browser);
}

SiteInstance* ExtensionProcessManager::GetSiteInstanceForURL(
    const GURL& url, Profile* profile) {
  BrowsingInstance* browsing_instance = GetBrowsingInstance(profile);
  return browsing_instance->GetSiteInstanceForURL(url);
}

void ExtensionProcessManager::DispatchEventToRenderers(Profile *profile,
    const std::string& event_name, const ListValue& data) {
  std::string json_data;
  JSONWriter::Write(&data, false, &json_data);

  std::set<int> process_ids = ExtensionMessageService::GetInstance(
      profile->GetRequestContext())->GetUniqueProcessIds();

  std::set<int>::iterator id;
  for (id = process_ids.begin(); id != process_ids.end(); ++id) {
    RenderProcessHost* rph = RenderProcessHost::FromID(*id);
    rph->Send(new ViewMsg_ExtensionHandleEvent(event_name, json_data));
  }
}

BrowsingInstance* ExtensionProcessManager::GetBrowsingInstance(
    Profile* profile) {
  BrowsingInstance* instance = browsing_instance_map_[profile];
  if (!instance) {
    instance = new BrowsingInstance(profile);
    browsing_instance_map_[profile] = instance;
  }
  return instance;
}
