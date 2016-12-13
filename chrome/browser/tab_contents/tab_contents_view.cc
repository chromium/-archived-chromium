// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tab_contents/tab_contents_view.h"

#include "chrome/browser/renderer_host/render_process_host.h"
#include "chrome/browser/renderer_host/render_widget_host.h"
#include "chrome/browser/renderer_host/render_widget_host_view.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/tab_contents/tab_contents_delegate.h"

TabContentsView::TabContentsView(TabContents* tab_contents)
    : tab_contents_(tab_contents),
      preferred_width_(0) {
}

void TabContentsView::CreateView() {
}

void TabContentsView::RenderWidgetHostDestroyed(RenderWidgetHost* host) {
  delegate_view_helper_.RenderWidgetHostDestroyed(host);
}

void TabContentsView::RenderViewCreated(RenderViewHost* host) {
  // Default implementation does nothing. Platforms may override.
}

void TabContentsView::UpdatePreferredWidth(int pref_width) {
  preferred_width_ = pref_width;
}

void TabContentsView::CreateNewWindow(int route_id,
                                      base::WaitableEvent* modal_dialog_event) {
  delegate_view_helper_.CreateNewWindow(route_id, modal_dialog_event,
                                        tab_contents_->profile(),
                                        tab_contents_->GetSiteInstance());
}

void TabContentsView::CreateNewWidget(int route_id, bool activatable) {
  CreateNewWidgetInternal(route_id, activatable);
}

void TabContentsView::ShowCreatedWindow(int route_id,
                                        WindowOpenDisposition disposition,
                                        const gfx::Rect& initial_pos,
                                        bool user_gesture,
                                        const GURL& creator_url) {
  TabContents* contents = delegate_view_helper_.GetCreatedWindow(route_id);
  if (contents) {
    tab_contents()->AddNewContents(contents, disposition, initial_pos,
                                   user_gesture, creator_url);
  }
}

void TabContentsView::ShowCreatedWidget(int route_id,
                                        const gfx::Rect& initial_pos) {
  RenderWidgetHostView* widget_host_view =
      delegate_view_helper_.GetCreatedWidget(route_id);
  ShowCreatedWidgetInternal(widget_host_view, initial_pos);
}

RenderWidgetHostView* TabContentsView::CreateNewWidgetInternal(
    int route_id, bool activatable) {
  return delegate_view_helper_.CreateNewWidget(route_id, activatable,
      tab_contents()->render_view_host()->process());
}

void TabContentsView::ShowCreatedWidgetInternal(
    RenderWidgetHostView* widget_host_view, const gfx::Rect& initial_pos) {
  if (tab_contents_->delegate())
    tab_contents_->delegate()->RenderWidgetShowing();

  widget_host_view->InitAsPopup(tab_contents_->render_widget_host_view(),
                                initial_pos);
  widget_host_view->GetRenderWidgetHost()->Init();
}
