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
  RenderViewID* render_view_id =
      ToolsContents::GetInspectedViewInfoAccessor()->GetProperty(
          property_bag());
  DCHECK(render_view_id);
  if (!render_view_id)
    return;

  render_view_host->SetupToolsClient(render_view_id->process_id,
                                     render_view_id->view_id);
  ToolsContents::GetInspectedViewInfoAccessor()->DeleteProperty(property_bag());
}

// static
bool ToolsContents::IsToolsUrl(const GURL& url) {
  // TODO(yurys): implement
  return (url.SchemeIs("chrome-ui") && url.host() == "inspector" &&
          url.path() == "/debugger-oop.html");
}

// static
PropertyAccessor<ToolsContents::RenderViewID>*
    ToolsContents::GetInspectedViewInfoAccessor() {
  return Singleton<PropertyAccessor<ToolsContents::RenderViewID> >().get();
}

