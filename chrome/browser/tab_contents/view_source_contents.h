// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIEW_SOURCE_CONTENTS_H__
#define CHROME_BROWSER_VIEW_SOURCE_CONTENTS_H__

#include "chrome/browser/tab_contents/web_contents.h"

// We use this class to implement view-source: URLs.
class ViewSourceContents : public WebContents {
 public:
  ViewSourceContents(Profile* profile, SiteInstance* instance);

 protected:
  // RenderViewHostDelegate overrides:
  virtual void RendererCreated(RenderViewHost* host);

  // WebContents overrides:
  // We override updating history with a no-op so these pages
  // are not saved to history.
  virtual void UpdateHistoryForNavigation(const GURL& url,
      const ViewHostMsg_FrameNavigate_Params& params) { }
};

#endif  // CHROME_BROWSER_VIEW_SOURCE_CONTENTS_H__

