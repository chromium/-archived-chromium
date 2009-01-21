// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TAB_CONTENTS_TEST_WEB_CONTENTS_H_
#define CHROME_BROWSER_TAB_CONTENTS_TEST_WEB_CONTENTS_H_

#include "chrome/browser/tab_contents/web_contents.h"

class RenderViewHostFactory;
class TestRenderViewHost;

// Subclass WebContents to ensure it creates TestRenderViewHosts and does
// not do anything involving views.
class TestWebContents : public WebContents {
 public:
  // The render view host factory will be passed on to the 
  TestWebContents(Profile* profile, SiteInstance* instance,
                  RenderViewHostFactory* rvh_factory);

  TestRenderViewHost* pending_rvh();

  // State accessor.
  bool cross_navigation_pending() {
    return render_manager_.cross_navigation_pending_;
  }

  // Overrides WebContents::ShouldTransitionCrossSite so that we can test both
  // alternatives without using command-line switches.
  bool ShouldTransitionCrossSite() { return transition_cross_site; }

  // Promote DidNavigate to public.
  void TestDidNavigate(RenderViewHost* render_view_host,
                       const ViewHostMsg_FrameNavigate_Params& params) {
    DidNavigate(render_view_host, params);
  }

  // Promote GetWebkitPrefs to public.
  WebPreferences TestGetWebkitPrefs() {
    return GetWebkitPrefs();
  }

  // Prevent interaction with views.
  bool CreateRenderViewForRenderManager(RenderViewHost* render_view_host) {
    // This will go to a TestRenderViewHost.
    render_view_host->CreateRenderView();
    return true;
  }
  void UpdateRenderViewSizeForRenderManager() {}

  // Set by individual tests.
  bool transition_cross_site;
};

#endif  // CHROME_BROWSER_TAB_CONTENTS_TEST_WEB_CONTENTS_H_
