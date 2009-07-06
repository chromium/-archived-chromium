// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_host/test/test_render_view_host.h"

#include "chrome/browser/renderer_host/backing_store.h"
#include "chrome/browser/tab_contents/test_web_contents.h"
#include "chrome/common/render_messages.h"

using webkit_glue::PasswordForm;

TestRenderViewHost::TestRenderViewHost(SiteInstance* instance,
                                       RenderViewHostDelegate* delegate,
                                       int routing_id,
                                       base::WaitableEvent* modal_dialog_event)
    : RenderViewHost(instance, delegate, routing_id, modal_dialog_event),
      render_view_created_(false),
      delete_counter_(NULL) {
  set_view(new TestRenderWidgetHostView(this));
}

TestRenderViewHost::~TestRenderViewHost() {
  if (delete_counter_)
    ++*delete_counter_;

  // Since this isn't a traditional view, we have to delete it.
  delete view();
}

bool TestRenderViewHost::CreateRenderView() {
  DCHECK(!render_view_created_);
  render_view_created_ = true;
  return true;
}

bool TestRenderViewHost::IsRenderViewLive() const {
  return render_view_created_;
}

void TestRenderViewHost::TestOnMessageReceived(const IPC::Message& msg) {
  OnMessageReceived(msg);
}

void TestRenderViewHost::SendNavigate(int page_id, const GURL& url) {
  ViewHostMsg_FrameNavigate_Params params;

  params.page_id = page_id;
  params.url = url;
  params.referrer = GURL::EmptyGURL();
  params.transition = PageTransition::LINK;
  params.redirects = std::vector<GURL>();
  params.should_update_history = true;
  params.searchable_form_url = GURL::EmptyGURL();
  params.searchable_form_element_name = std::wstring();
  params.searchable_form_encoding = std::string();
  params.password_form = PasswordForm();
  params.security_info = std::string();
  params.gesture = NavigationGestureUser;
  params.contents_mime_type = std::string();
  params.is_post = false;
  params.is_content_filtered = false;
  params.http_status_code = 0;

  ViewHostMsg_FrameNavigate msg(1, params);
  OnMsgNavigate(msg);
}

TestRenderWidgetHostView::TestRenderWidgetHostView(RenderWidgetHost* rwh)
    : rwh_(rwh),
      is_showing_(false) {
}

BackingStore* TestRenderWidgetHostView::AllocBackingStore(
    const gfx::Size& size) {
  return new BackingStore(rwh_, size);
}

void RenderViewHostTestHarness::NavigateAndCommit(const GURL& url) {
  controller().LoadURL(url, GURL(), 0);
  rvh()->SendNavigate(process()->max_page_id() + 1, url);
}

void RenderViewHostTestHarness::SetUp() {
  // See comment above profile_ decl for why we check for NULL here.
  if (!profile_.get())
    profile_.reset(new TestingProfile());

  // This will be deleted when the TabContents goes away.
  SiteInstance* instance = SiteInstance::CreateSiteInstance(profile_.get());

  contents_.reset(new TestTabContents(profile_.get(), instance));
}

void RenderViewHostTestHarness::TearDown() {
  contents_.reset();

  // Make sure that we flush any messages related to TabContents destruction
  // before we destroy the profile.
  MessageLoop::current()->RunAllPending();
}
