// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tab_contents/web_contents_view.h"

#include "chrome/browser/renderer_host/render_widget_host.h"

void WebContentsView::RenderWidgetHostDestroyed(RenderWidgetHost* host) {
  for (PendingWidgetViews::iterator i = pending_widget_views_.begin();
       i != pending_widget_views_.end(); ++i) {
    if (host->view() == i->second) {
      pending_widget_views_.erase(i);
      return;
    }
  }
}

void WebContentsView::CreateNewWindow(int route_id,
                                      base::WaitableEvent* modal_dialog_event) {
  // Save the created window associated with the route so we can show it later.
  pending_contents_[route_id] = CreateNewWindowInternal(route_id,
                                                        modal_dialog_event);
}

void WebContentsView::CreateNewWidget(int route_id, bool activatable) {
  // Save the created widget associated with the route so we can show it later.
  pending_widget_views_[route_id] = CreateNewWidgetInternal(route_id,
                                                            activatable);
}

void WebContentsView::ShowCreatedWindow(int route_id,
                                        WindowOpenDisposition disposition,
                                        const gfx::Rect& initial_pos,
                                        bool user_gesture) {
  PendingContents::iterator iter = pending_contents_.find(route_id);
  if (iter == pending_contents_.end()) {
    DCHECK(false);
    return;
  }

  WebContents* new_web_contents = iter->second;
  pending_contents_.erase(route_id);

  ShowCreatedWindowInternal(new_web_contents, disposition, initial_pos,
                            user_gesture);
}

void WebContentsView::ShowCreatedWidget(int route_id,
                                        const gfx::Rect& initial_pos) {
  PendingWidgetViews::iterator iter = pending_widget_views_.find(route_id);
  if (iter == pending_widget_views_.end()) {
    DCHECK(false);
    return;
  }

  RenderWidgetHostView* widget_host_view = iter->second;
  pending_widget_views_.erase(route_id);

  ShowCreatedWidgetInternal(widget_host_view, initial_pos);
}
