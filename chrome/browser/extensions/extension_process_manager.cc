// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_process_manager.h"

#include "base/singleton.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/extensions/extension_view.h"
#include "chrome/browser/tab_contents/site_instance.h"

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

BrowsingInstance* ExtensionProcessManager::GetBrowsingInstance(
    Profile* profile) {
  BrowsingInstance* instance = browsing_instance_map_[profile];
  if (!instance) {
    instance = new BrowsingInstance(profile);
    browsing_instance_map_[profile] = instance;
  }
  return instance;
}
