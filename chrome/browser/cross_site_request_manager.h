// Copyright 2008, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#ifndef CHROME_BROWSER_CROSS_SITE_REQUEST_MANAGER_H__
#define CHROME_BROWSER_CROSS_SITE_REQUEST_MANAGER_H__

#include <set>
#include <utility>

#include "base/basictypes.h"
#include "base/lock.h"
#include "base/singleton.h"

// CrossSiteRequestManager is used to handle bookkeeping for cross-site
// requests and responses between the UI and IO threads.  Such requests involve
// a transition from one RenderViewHost to another within WebContents, and
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
  friend DefaultSingletonTraits<CrossSiteRequestManager>;
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
