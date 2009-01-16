// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tab_contents/tab_util.h"

#include "chrome/browser/render_view_host.h"
#include "chrome/browser/renderer_host/render_process_host.h"
#include "chrome/browser/renderer_host/resource_dispatcher_host.h"
#include "chrome/browser/tab_contents/web_contents.h"
#include "net/url_request/url_request.h"

bool tab_util::GetTabContentsID(URLRequest* request,
                                int* render_process_id,
                                int* render_view_id) {

  if (!request || !render_process_id || !render_view_id)
    return false;

  ResourceDispatcherHost::ExtraRequestInfo* info =
      ResourceDispatcherHost::ExtraInfoForRequest(request);
  if (!info)
    return false;

  *render_process_id = info->render_process_host_id;
  *render_view_id = info->render_view_id;
  return true;
}

WebContents* tab_util::GetWebContentsByID(int render_process_id,
                                          int render_view_id) {
  RenderViewHost* render_view_host =
      RenderViewHost::FromID(render_process_id, render_view_id);
  if (!render_view_host)
    return NULL;

  return static_cast<WebContents*>(render_view_host->delegate());
}

