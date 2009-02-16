// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEBUGGER_TOOLS_CONTENTS_H_
#define CHROME_BROWSER_DEBUGGER_TOOLS_CONTENTS_H_

#include "chrome/browser/tab_contents/web_contents.h"
#include "chrome/common/property_bag.h"

// TODO(yurys): it may be made into DOMUI if the latter supports RendererCreated
// notification.
class ToolsContents : public WebContents {
 public:
  ToolsContents(Profile* profile, SiteInstance* instance);

  static bool IsToolsUrl(const GURL& url);

  // (render process id, render view id)
  static PropertyAccessor<std::pair<int, int> >* GetInspectedViewInfoAccessor();

 protected:
  // WebContents overrides:
  // We override updating history with a no-op so these pages
  // are not saved to history.
  virtual void UpdateHistoryForNavigation(const GURL& url,  
      const ViewHostMsg_FrameNavigate_Params& params) { }

  // Will notify just created renderer that it's going to host developer 
  // tools UI.
  virtual void RendererCreated(RenderViewHost* render_view_host);

  DISALLOW_COPY_AND_ASSIGN(ToolsContents);
};

#endif  // CHROME_BROWSER_DEBUGGER_TOOLS_CONTENTS_H_
