// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CROSS_SITE_REQUEST_MANAGER_H__
#define CHROME_BROWSER_CROSS_SITE_REQUEST_MANAGER_H__

#include <set>
#include <utility>

#include "base/basictypes.h"
#include "base/lock.h"
#include "base/singleton.h"

// CrossSiteRequestManager is used to handle bookkeeping for cross-site
// requests and responses between the UI and IO threads.  Such requests involve
// a transition from one RenderViewHost to another within TabContents, and
// involve coordination with ResourceDispatcherHost.
//
// CrossSiteRequestManager is a singleton that may be used on any thread.
//
class CrossSiteRequestManager {
 public:
  // Returns whether the RenderViewHost specified by the given IDs currently
  // has a pending cross-site request.  If so, we will have to delay the
  // response until the previous RenderViewHost runs its onunload handler.
  // Called by ResourceDispatcherHost on the IO thread.
  bool HasPendingCrossSiteRequest(int renderer_id, int render_view_id);

  // Sets whether the RenderViewHost specified by the given IDs currently has a
  // pending cross-site request.  Called by RenderViewHost on the UI thread.
  void SetHasPendingCrossSiteRequest(int renderer_id,
                                     int render_view_id,
                                     bool has_pending);

 private:
  friend struct DefaultSingletonTraits<CrossSiteRequestManager>;
  typedef std::set<std::pair<int, int> > RenderViewSet;

  // Obtain an instance of CrossSiteRequestManager via
  // Singleton<CrossSiteRequestManager>().
  CrossSiteRequestManager() {}

  // You must acquire this lock before reading or writing any members of this
  // class.  You must not block while holding this lock.
  Lock lock_;

  // Set of (render_process_host_id, render_view_id) pairs of all
  // RenderViewHosts that have pending cross-site requests.  Used to pass
  // information about the RenderViewHosts between the UI and IO threads.
  RenderViewSet pending_cross_site_views_;

  DISALLOW_EVIL_CONSTRUCTORS(CrossSiteRequestManager);
};

#endif  // CHROME_BROWSER_CROSS_SITE_REQUEST_MANAGER_H__
