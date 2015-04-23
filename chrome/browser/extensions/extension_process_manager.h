// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_PROCESS_MANAGER_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_PROCESS_MANAGER_H_

#include <set>

#include "base/ref_counted.h"
#include "chrome/common/notification_registrar.h"

class Browser;
class BrowsingInstance;
class Extension;
class ExtensionHost;
#if defined(TOOLKIT_VIEWS)
class ExtensionView;
#endif
class GURL;
class Profile;
class SiteInstance;

// Manages dynamic state of running Chromium extensions.  There is one instance
// of this class per Profile (including OTR).
class ExtensionProcessManager : public NotificationObserver {
 public:
  explicit ExtensionProcessManager(Profile* profile);
  ~ExtensionProcessManager();

  // Creates a new ExtensionHost with its associated view, grouping it in the
  // appropriate SiteInstance (and therefore process) based on the URL and
  // profile.
  ExtensionHost* CreateView(Extension* extension,
                            const GURL& url,
                            Browser* browser);
  ExtensionHost* CreateView(const GURL& url, Browser* browser);

  // Creates a new UI-less extension instance.  Like CreateView, but not
  // displayed anywhere.
  ExtensionHost* CreateBackgroundHost(Extension* extension, const GURL& url);

  // Returns the SiteInstance that the given URL belongs to.
  SiteInstance* GetSiteInstanceForURL(const GURL& url);

  // NotificationObserver:
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  typedef std::set<ExtensionHost*> ExtensionHostSet;
  typedef ExtensionHostSet::const_iterator const_iterator;
  const_iterator begin() const { return all_hosts_.begin(); }
  const_iterator end() const { return all_hosts_.end(); }

 private:
  // Called just after |host| is created so it can be registered in our lists.
  void OnExtensionHostCreated(ExtensionHost* host, bool is_background);

  NotificationRegistrar registrar_;

  // The set of all ExtensionHosts managed by this process manager.
  ExtensionHostSet all_hosts_;

  // The set of running viewless background extensions.
  ExtensionHostSet background_hosts_;

  // The BrowsingInstance shared by all extensions in this profile.  This
  // controls process grouping.
  scoped_refptr<BrowsingInstance> browsing_instance_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionProcessManager);
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_PROCESS_MANAGER_H_
