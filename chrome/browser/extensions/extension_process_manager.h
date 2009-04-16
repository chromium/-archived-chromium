// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_PROCESS_MANAGER_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_PROCESS_MANAGER_H_

#include "base/ref_counted.h"

#include <map>
#include <string>

class Browser;
class BrowsingInstance;
class Extension;
class ExtensionView;
class GURL;
class ListValue;
class Profile;
class SiteInstance;

// This class controls what process new extension instances use.  We use the
// BrowsingInstance site grouping rules to group extensions.  Since all
// resources in a given extension have the same origin, they will be grouped
// into the same process.
//
// We separate further by Profile: each Profile has its own group of extension
// processes that never overlap with other Profiles.
class ExtensionProcessManager {
 public:
  static ExtensionProcessManager* GetInstance();

  // These are public for use by Singleton.  Do not instantiate or delete
  // manually.
  ExtensionProcessManager();
  ~ExtensionProcessManager();

  // Creates a new ExtensionView, grouping it in the appropriate SiteInstance
  // (and therefore process) based on the URL and profile.
  ExtensionView* CreateView(Extension* extension,
                            const GURL& url,
                            Browser* browser);

  // Returns the SiteInstance that the given URL belongs to in this profile.
  SiteInstance* GetSiteInstanceForURL(const GURL& url, Profile* profile);

  // Sends the event to each renderer process within the current profile that
  // contain at least one extension renderer.
  void DispatchEventToRenderers(Profile *profile,
                                const std::string& event_name,
                                const ListValue& data);
 private:
  // Returns our BrowsingInstance for the given profile.  Lazily created and
  // cached.
  BrowsingInstance* GetBrowsingInstance(Profile* profile);

  // Cache of BrowsingInstances grouped by Profile.
  typedef std::map<Profile*, scoped_refptr<BrowsingInstance> >
      BrowsingInstanceMap;
  BrowsingInstanceMap browsing_instance_map_;
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_PROCESS_MANAGER_H_
