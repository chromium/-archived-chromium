// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_process_manager.h"

#include "chrome/browser/browsing_instance.h"
#include "chrome/browser/extensions/extension_host.h"
#if defined(TOOLKIT_VIEWS)
#include "chrome/browser/extensions/extension_view.h"
#endif
#include "chrome/browser/extensions/extensions_service.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/tab_contents/site_instance.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/notification_service.h"

static void CreateBackgroundHosts(
    ExtensionProcessManager* manager, const ExtensionList* extensions) {
  for (ExtensionList::const_iterator extension = extensions->begin();
       extension != extensions->end(); ++extension) {
    // Start the process for the master page, if it exists.
    if ((*extension)->background_url().is_valid())
      manager->CreateBackgroundHost(*extension, (*extension)->background_url());
  }
}

ExtensionProcessManager::ExtensionProcessManager(Profile* profile)
    : browsing_instance_(new BrowsingInstance(profile)) {
  registrar_.Add(this, NotificationType::EXTENSIONS_LOADED,
                 NotificationService::AllSources());
  registrar_.Add(this, NotificationType::EXTENSION_UNLOADED,
                 NotificationService::AllSources());

  if (profile->GetExtensionsService())
    CreateBackgroundHosts(this, profile->GetExtensionsService()->extensions());
}

ExtensionProcessManager::~ExtensionProcessManager() {
  for (ExtensionHostList::iterator iter = background_hosts_.begin();
      iter != background_hosts_.end(); ++iter)
    delete *iter;
}

#if defined(TOOLKIT_VIEWS)
ExtensionView* ExtensionProcessManager::CreateView(Extension* extension,
                                                   const GURL& url,
                                                   Browser* browser) {
  return new ExtensionView(
      new ExtensionHost(extension, GetSiteInstanceForURL(url)), browser, url);
}
#endif

void ExtensionProcessManager::CreateBackgroundHost(Extension* extension,
                                                   const GURL& url) {
  ExtensionHost* host =
      new ExtensionHost(extension, GetSiteInstanceForURL(url));
  host->CreateRenderView(url, NULL);  // create a RenderViewHost with no view
  background_hosts_.push_back(host);
}

SiteInstance* ExtensionProcessManager::GetSiteInstanceForURL(const GURL& url) {
  return browsing_instance_->GetSiteInstanceForURL(url);
}

void ExtensionProcessManager::Observe(NotificationType type,
                                      const NotificationSource& source,
                                      const NotificationDetails& details) {
  switch (type.value) {
    case NotificationType::EXTENSIONS_LOADED: {
      const ExtensionList* extensions = Details<ExtensionList>(details).ptr();
      CreateBackgroundHosts(this, extensions);
      break;
    }

    case NotificationType::EXTENSION_UNLOADED: {
      Extension* extension = Details<Extension>(details).ptr();
      for (ExtensionHostList::iterator iter = background_hosts_.begin();
           iter != background_hosts_.end(); ++iter) {
        ExtensionHost* host = *iter;
        if (host->extension()->id() == extension->id()) {
          background_hosts_.erase(iter);
          delete host;
          break;
        }
      }
      break;
    }

    default:
      NOTREACHED();
  }
}
