// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/hwnd_html_view.h"

#include "chrome/browser/render_view_host.h"
#include "chrome/browser/render_widget_host_view_win.h"
#include "chrome/browser/render_view_host_delegate.h"
#include "chrome/browser/tab_contents/site_instance.h"
#include "chrome/views/widget.h"

HWNDHtmlView::~HWNDHtmlView() {
  if (render_view_host_) {
    Detach();
    render_view_host_->Shutdown();
    render_view_host_ = NULL;
  }
}

void HWNDHtmlView::Init(HWND parent_hwnd) {
  DCHECK(!render_view_host_) << "Already initialized.";
  RenderViewHost* rvh = new RenderViewHost(
    SiteInstance::CreateSiteInstance(delegate_->GetProfile()),
    delegate_, MSG_ROUTING_NONE, NULL);
  render_view_host_ = rvh;

  RenderWidgetHostViewWin* view = new RenderWidgetHostViewWin(rvh);
  rvh->set_view(view);

  // Create the HWND. Note:
  // RenderWidgetHostHWND supports windowed plugins, but if we ever also wanted
  // to support constrained windows with this, we would need an additional
  // HWND to parent off of because windowed plugin HWNDs cannot exist in the
  // same z-order as constrained windows.
  HWND hwnd = view->Create(parent_hwnd);
  view->ShowWindow(SW_SHOW);
  views::HWNDView::Attach(hwnd);

  // Start up the renderer.
  if (allow_dom_ui_bindings_)
    rvh->AllowDOMUIBindings();
  rvh->CreateRenderView();
  rvh->NavigateToURL(content_url_);
  initialized_ = true;
}

void HWNDHtmlView::ViewHierarchyChanged(bool is_add, View* parent,
                                        View* child) {
  if (is_add && GetWidget() && !initialized_)
    Init(GetWidget()->GetHWND());
}
