// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TAB_CONTENTS_RENDER_VIEW_HOST_DELEGATE_HELPER_H_
#define CHROME_BROWSER_TAB_CONTENTS_RENDER_VIEW_HOST_DELEGATE_HELPER_H_

#include <map>

#include "base/basictypes.h"
#include "base/gfx/rect.h"
#include "base/waitable_event.h"
#include "webkit/glue/webpreferences.h"
#include "webkit/glue/window_open_disposition.h"

class Browser;
class PrefService;
class Profile;
class RenderProcessHost;
class RenderViewHost;
class RenderWidgetHost;
class RenderWidgetHostView;
class SiteInstance;
class TabContents;

// Provides helper methods that provide common implementations of some
// RenderViewHostDelegate::View methods.
class RenderViewHostDelegateViewHelper {
 public:
  RenderViewHostDelegateViewHelper() {}

  virtual void CreateNewWindow(int route_id,
                               base::WaitableEvent* modal_dialog_event,
                               Profile* profile, SiteInstance* site);
  virtual RenderWidgetHostView* CreateNewWidget(int route_id, bool activatable,
                                                RenderProcessHost* process);
  virtual TabContents* GetCreatedWindow(int route_id);
  virtual RenderWidgetHostView* GetCreatedWidget(int route_id);
  void RenderWidgetHostDestroyed(RenderWidgetHost* host);

 private:
  // Tracks created TabContents objects that have not been shown yet. They are
  // identified by the route ID passed to CreateNewWindow.
  typedef std::map<int, TabContents*> PendingContents;
  PendingContents pending_contents_;

  // These maps hold on to the widgets that we created on behalf of the
  // renderer that haven't shown yet.
  typedef std::map<int, RenderWidgetHostView*> PendingWidgetViews;
  PendingWidgetViews pending_widget_views_;

  DISALLOW_COPY_AND_ASSIGN(RenderViewHostDelegateViewHelper);
};


// Provides helper methods that provide common implementations of some
// RenderViewHostDelegate methods.
class RenderViewHostDelegateHelper {
 public:
  static WebPreferences GetWebkitPrefs(PrefService* prefs, bool is_dom_ui);

 private:
  RenderViewHostDelegateHelper();

  DISALLOW_COPY_AND_ASSIGN(RenderViewHostDelegateHelper);
};

#endif  // CHROME_BROWSER_TAB_CONTENTS_RENDER_VIEW_HOST_DELEGATE_HELPER_H_
