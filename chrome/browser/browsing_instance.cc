// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browsing_instance.h"

#include "base/command_line.h"
#include "chrome/browser/tab_contents/site_instance.h"
#include "chrome/common/chrome_switches.h"

/*static*/
BrowsingInstance::ProfileSiteInstanceMap
    BrowsingInstance::profile_site_instance_map_;

bool BrowsingInstance::ShouldUseProcessPerSite(const GURL& url) {
  // Returns true if we should use the process-per-site model.  This will be
  // the case if the --process-per-site switch is specified, or in
  // process-per-site-instance for particular sites (e.g., the new tab page).

  const CommandLine& command_line = *CommandLine::ForCurrentProcess();
  if (command_line.HasSwitch(switches::kProcessPerSite))
    return true;

  if (!command_line.HasSwitch(switches::kProcessPerTab)) {
    // We are not in process-per-site or process-per-tab, so we must be in the
    // default (process-per-site-instance).  Only use the process-per-site
    // logic for particular sites that we want to consolidate.
    // Note that --single-process may have been specified, but that affects the
    // process creation logic in RenderProcessHost, so we do not need to worry
    // about it here.
    if (url.SchemeIs("chrome"))
      // Always consolidate instances of the new tab page (and instances of any
      // other internal resource urls).
      return true;

    // TODO(creis): List any other special cases that we want to limit to a
    // single process for all instances.
  }

  // In all other cases, don't use process-per-site logic.
  return false;
}

BrowsingInstance::SiteInstanceMap* BrowsingInstance::GetSiteInstanceMap(
    Profile* profile, const GURL& url) {
  if (!ShouldUseProcessPerSite(url)) {
    // Not using process-per-site, so use a map specific to this instance.
    return &site_instance_map_;
  }

  // Otherwise, process-per-site is in use, at least for this URL.  Look up the
  // global map for this profile, creating an entry if necessary.
  return &profile_site_instance_map_[profile];
}

bool BrowsingInstance::HasSiteInstance(const GURL& url) {
  std::string site = SiteInstance::GetSiteForURL(url).possibly_invalid_spec();

  SiteInstanceMap* map = GetSiteInstanceMap(profile_, url);
  SiteInstanceMap::iterator i = map->find(site);
  return (i != map->end());
}

SiteInstance* BrowsingInstance::GetSiteInstanceForURL(const GURL& url) {
  std::string site = SiteInstance::GetSiteForURL(url).possibly_invalid_spec();

  SiteInstanceMap* map = GetSiteInstanceMap(profile_, url);
  SiteInstanceMap::iterator i = map->find(site);
  if (i != map->end()) {
    return i->second;
  }

  // No current SiteInstance for this site, so let's create one.
  SiteInstance* instance = new SiteInstance(this);

  // Set the site of this new SiteInstance, which will register it with us.
  instance->SetSite(url);
  return instance;
}

void BrowsingInstance::RegisterSiteInstance(SiteInstance* site_instance) {
  DCHECK(site_instance->browsing_instance() == this);
  DCHECK(site_instance->has_site());
  std::string site = site_instance->site().possibly_invalid_spec();

  // Only register if we don't have a SiteInstance for this site already.
  // It's possible to have two SiteInstances point to the same site if two
  // tabs are navigated there at the same time.  (We don't call SetSite or
  // register them until DidNavigate.)  If there is a previously existing
  // SiteInstance for this site, we just won't register the new one.
  SiteInstanceMap* map = GetSiteInstanceMap(profile_, site_instance->site());
  SiteInstanceMap::iterator i = map->find(site);
  if (i == map->end()) {
    // Not previously registered, so register it.
    (*map)[site] = site_instance;
  }
}

void BrowsingInstance::UnregisterSiteInstance(SiteInstance* site_instance) {
  DCHECK(site_instance->browsing_instance() == this);
  DCHECK(site_instance->has_site());
  std::string site = site_instance->site().possibly_invalid_spec();

  // Only unregister the SiteInstance if it is the same one that is registered
  // for the site.  (It might have been an unregistered SiteInstance.  See the
  // comments in RegisterSiteInstance.)
  SiteInstanceMap* map = GetSiteInstanceMap(profile_, site_instance->site());
  SiteInstanceMap::iterator i = map->find(site);
  if (i != map->end() && i->second == site_instance) {
    // Matches, so erase it.
    map->erase(i);
  }
}

