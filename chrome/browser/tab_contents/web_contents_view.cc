// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tab_contents/web_contents_view.h"

#include "chrome/browser/renderer_host/render_process_host.h"
#include "chrome/browser/renderer_host/render_widget_host.h"
#include "chrome/browser/renderer_host/render_widget_host_view.h"
#include "chrome/browser/tab_contents/tab_contents_delegate.h"
#include "chrome/browser/tab_contents/web_contents.h"

WebContentsView::WebContentsView(WebContents* web_contents) 
    : web_contents_(web_contents) {
}

void WebContentsView::CreateView() {
}

void WebContentsView::RenderWidgetHostDestroyed(RenderWidgetHost* host) {
  delegate_view_helper_.RenderWidgetHostDestroyed(host);
}

void WebContentsView::CreateNewWindow(int route_id,
                                      base::WaitableEvent* modal_dialog_event) {
  delegate_view_helper_.CreateNewWindow(route_id, modal_dialog_event,
                                        web_contents_->profile(),
                                        web_contents_->GetSiteInstance());
}

void WebContentsView::CreateNewWidget(int route_id, bool activatable) {
  CreateNewWidgetInternal(route_id, activatable);
}

void WebContentsView::ShowCreatedWindow(int route_id,
                                        WindowOpenDisposition disposition,
                                        const gfx::Rect& initial_pos,
                                        bool user_gesture) {
  WebContents* contents = delegate_view_helper_.GetCreatedWindow(route_id);
  if (contents) {
    web_contents()->AddNewContents(contents, disposition, initial_pos,
                                   user_gesture);
  }
}

void WebContentsView::ShowCreatedWidget(int route_id,
                                        const gfx::Rect& initial_pos) {
  RenderWidgetHostView* widget_host_view =
      delegate_view_helper_.GetCreatedWidget(route_id);
  ShowCreatedWidgetInternal(widget_host_view, initial_pos);
}

RenderWidgetHostView* WebContentsView::CreateNewWidgetInternal(
    int route_id, bool activatable) {
  return delegate_view_helper_.CreateNewWidget(route_id, activatable,
      web_contents()->render_view_host()->process());
}

void WebContentsView::ShowCreatedWidgetInternal(
    RenderWidgetHostView* widget_host_view, const gfx::Rect& initial_pos) {
  if (web_contents_->delegate())
    web_contents_->delegate()->RenderWidgetShowing();

  widget_host_view->InitAsPopup(web_contents_->render_widget_host_view(),
                                initial_pos);
  widget_host_view->GetRenderWidgetHost()->Init();
}

