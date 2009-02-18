// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/debugger/tools_contents.h"

#include "base/singleton.h"

ToolsContents::ToolsContents(Profile* profile, SiteInstance* instance)
    : WebContents(profile,
                  instance,
                  NULL,
                  MSG_ROUTING_NONE,
                  NULL) {
  set_type(TAB_CONTENTS_TOOLS);
}

void ToolsContents::RendererCreated(RenderViewHost* render_view_host)  {
  std::pair<int, int>* render_view_info =
      ToolsContents::GetInspectedViewInfoAccessor()->GetProperty(
          property_bag());
  DCHECK(render_view_info);
  if (!render_view_info)
    return;

  render_view_host->SetUpToolsClient(render_view_info->first,
                                     render_view_info->second);
  ToolsContents::GetInspectedViewInfoAccessor()->DeleteProperty(property_bag());
}

// static
bool ToolsContents::IsToolsUrl(const GURL& url) {
  // TODO(yurys): implement
  return (url.SchemeIs("chrome-ui") && url.host() == "inspector" &&
          url.path() == "/debugger-oop.html");
}

// static
PropertyAccessor<std::pair<int, int> >*
    ToolsContents::GetInspectedViewInfoAccessor() {
  return Singleton<PropertyAccessor<std::pair<int, int> > >().get();
}

