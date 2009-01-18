// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_host/test_render_view_host.h"

void TestRenderViewHost::SendNavigate(int page_id, const GURL& url) {
  ViewHostMsg_FrameNavigate_Params params;
  params.is_post = false;
  params.page_id = page_id;
  params.is_content_filtered = false;
  params.url = url;
  params.should_update_history = true;
  params.transition = PageTransition::LINK;
  params.is_post = false;

  ViewHostMsg_FrameNavigate msg(1, params);
  OnMsgNavigate(msg);
}

void RenderViewHostTestHarness::SetUp() {
  // This will be deleted when the WebContents goes away.
  SiteInstance* instance = SiteInstance::CreateSiteInstance(&profile_);

  // Make the SiteInstance use our RenderProcessHost as its own.
  process_ = new MockRenderProcessHost(&profile_);
  instance->set_process_host_id(process_->host_id());

  contents_ = new WebContents(&profile_, instance, &rvh_factory_, 12, NULL);
  controller_ = new NavigationController(contents_, &profile_);
}

void RenderViewHostTestHarness::TearDown() {
  contents_->CloseContents();
  contents_ = NULL;
  controller_ = NULL;

  // Make sure that we flush any messages related to WebContents destruction
  // before we destroy the profile.
  MessageLoop::current()->RunAllPending();
}
