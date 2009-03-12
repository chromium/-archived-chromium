// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TAB_CONTENTS_SITE_INSTANCE_H_
#define CHROME_BROWSER_TAB_CONTENTS_SITE_INSTANCE_H_

#include "chrome/browser/browsing_instance.h"
#include "chrome/browser/renderer_host/render_process_host.h"
#include "chrome/common/notification_observer.h"
#include "googleurl/src/gurl.h"

///////////////////////////////////////////////////////////////////////////////
//
// SiteInstance class
//
// A SiteInstance is a data structure that is associated with all pages in a
// given instance of a web site.  Here, a web site is identified by its
// registered domain name and scheme.  An instance includes all pages
// that are connected (i.e., either a user or a script navigated from one
// to the other).  We represent instances using the BrowsingInstance class.
//
// In --process-per-tab, one SiteInstance is created for each tab (i.e., in the
// WebContents constructor), unless the tab is created by script (i.e., in
// WebContents::CreateNewView).  This corresponds to one process per
// BrowsingInstance.
//
// In process-per-site-instance (the current default process model),
// SiteInstances are created (1) when the user manually creates a new tab
// (which also creates a new BrowsingInstance), and (2) when the user navigates
// across site boundaries (which uses the same BrowsingInstance).  If the user
// navigates within a site, or opens links in new tabs within a site, the same
// SiteInstance is used.
//
// In --process-per-site, we consolidate all SiteInstances for a given site,
// throughout the entire profile.  This ensures that only one process will be
// dedicated to each site.
//
// Each NavigationEntry for a WebContents points to the SiteInstance that
// rendered it.  Each RenderViewHost also points to the SiteInstance that it is
// associated with.  A SiteInstance keeps track of the number of these
// references and deletes itself when the count goes to zero.  This means that
// a SiteInstance is only live as long as it is accessible, either from new
// tabs with no NavigationEntries or in NavigationEntries in the history.
//
///////////////////////////////////////////////////////////////////////////////
class SiteInstance : public base::RefCounted<SiteInstance>,
                     public NotificationObserver {
 public:
  // Virtual to allow tests to extend it.
  virtual ~SiteInstance();

  // Get the BrowsingInstance to which this SiteInstance belongs.
  BrowsingInstance* browsing_instance() { return browsing_instance_; }

  // Sets the factory used to create new RenderProcessHosts. This will also be
  // passed on to SiteInstances spawned by this one.
  //
  // The factory must outlive the SiteInstance; ownership is not transferred. It
  // may be NULL, in which case the default BrowserRenderProcessHost will be
  // created (this is the behavior if you don't call this function).
  void set_render_process_host_factory(RenderProcessHostFactory* rph_factory) {
    render_process_host_factory_ = rph_factory;
  }

  // Update / Get the max page ID for this SiteInstance.
  void UpdateMaxPageID(int32 page_id) {
    if (page_id > max_page_id_)
      max_page_id_ = page_id;
  }
  int32 max_page_id() const { return max_page_id_; }

  // Returns the current process being used to render pages in this
  // SiteInstance.  If the process has crashed or otherwise gone away, then
  // this method will create a new process and update our host ID accordingly.
  RenderProcessHost* GetProcess();

  // Set / Get the web site that this SiteInstance is rendering pages for.
  // This includes the scheme and registered domain, but not the port.  If the
  // URL does not have a valid registered domain, then the full hostname is
  // stored.
  void SetSite(const GURL& url);
  const GURL& site() const { return site_; }
  bool has_site() const { return has_site_; }

  // Returns whether there is currently a related SiteInstance (registered with
  // BrowsingInstance) for the site of the given url.  If so, we should try to
  // avoid dedicating an unused SiteInstance to it (e.g., in a new tab).
  bool HasRelatedSiteInstance(const GURL& url);

  // Gets a SiteInstance for the given URL that shares the current
  // BrowsingInstance, creating a new SiteInstance if necessary.  This ensures
  // that a BrowsingInstance only has one SiteInstance per site, so that pages
  // in a BrowsingInstance have the ability to script each other.  Callers
  // should ensure that this SiteInstance becomes ref counted, by storing it in
  // a scoped_refptr.  (By having this method, we can hide the BrowsingInstance
  // class from the rest of the codebase.)
  // TODO(creis): This may be an argument to build a pass_refptr<T> class, as
  // Darin suggests.
  SiteInstance* GetRelatedSiteInstance(const GURL& url);

  // Factory method to create a new SiteInstance.  This will create a new
  // new BrowsingInstance, so it should only be used when creating a new tab
  // from scratch (or similar circumstances).  Callers should ensure that
  // this SiteInstance becomes ref counted, by storing it in a scoped_refptr.
  //
  // The render process host factory may be NULL. See SiteInstance constructor.
  //
  // TODO(creis): This may be an argument to build a pass_refptr<T> class, as
  // Darin suggests.
  static SiteInstance* CreateSiteInstance(Profile* profile);

  // Returns the site for the given URL, which includes only the scheme and
  // registered domain.  Returns an empty GURL if the URL has no host.
  static GURL GetSiteForURL(const GURL& url);

  // Return whether both URLs are part of the same web site, for the purpose of
  // assigning them to processes accordingly.  The decision is currently based
  // on the registered domain of the URLs (google.com, bbc.co.uk), as well as
  // the scheme (https, http).  This ensures that two pages will be in
  // the same process if they can communicate with other via JavaScript.
  // (e.g., docs.google.com and mail.google.com have DOM access to each other
  // if they both set their document.domain properties to google.com.)
  static bool IsSameWebSite(const GURL& url1, const GURL& url2);

 protected:
  friend class BrowsingInstance;

  // Create a new SiteInstance.  Protected to give access to BrowsingInstance
  // and tests; most callers should use CreateSiteInstance or
  // GetRelatedSiteInstance instead.
  SiteInstance(BrowsingInstance* browsing_instance);

 private:
  // NotificationObserver implementation.
  void Observe(NotificationType type,
               const NotificationSource& source,
               const NotificationDetails& details);

  // BrowsingInstance to which this SiteInstance belongs.
  scoped_refptr<BrowsingInstance> browsing_instance_;

  // Factory for new RenderProcessHosts, not owned by this class. NULL indiactes
  // that the default BrowserRenderProcessHost should be created.
  const RenderProcessHostFactory* render_process_host_factory_;

  // Current RenderProcessHost that is rendering pages for this SiteInstance.
  RenderProcessHost* process_;

  // The current max_page_id in the SiteInstance's RenderProcessHost.  If the
  // rendering process dies, its replacement should start issuing page IDs that
  // are larger than this value.
  int32 max_page_id_;

  // The web site that this SiteInstance is rendering pages for.
  GURL site_;

  // Whether SetSite has been called.
  bool has_site_;

  DISALLOW_EVIL_CONSTRUCTORS(SiteInstance);
};

#endif  // CHROME_BROWSER_TAB_CONTENTS_SITE_INSTANCE_H_
