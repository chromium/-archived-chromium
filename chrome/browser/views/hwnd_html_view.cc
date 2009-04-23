// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/hwnd_html_view.h"

#include "chrome/browser/renderer_host/render_view_host.h"
#include "chrome/browser/renderer_host/render_widget_host_view_win.h"
#include "chrome/browser/renderer_host/render_view_host_delegate.h"
#include "chrome/browser/tab_contents/site_instance.h"
#include "chrome/views/widget/widget.h"
#include "chrome/views/widget/widget_win.h"

HWNDHtmlView::HWNDHtmlView(const GURL& content_url,
                           RenderViewHostDelegate* delegate,
                           bool allow_dom_ui_bindings, SiteInstance* instance)
    : render_view_host_(NULL),
      content_url_(content_url),
      allow_dom_ui_bindings_(allow_dom_ui_bindings),
      delegate_(delegate),
      initialized_(false),
      site_instance_(instance) {
  if (!site_instance_)
    site_instance_ = SiteInstance::CreateSiteInstance(delegate_->GetProfile());
}

HWNDHtmlView::~HWNDHtmlView() {
  if (render_view_host_) {
    Detach();
    render_view_host_->Shutdown();
    render_view_host_ = NULL;
  }
}

void HWNDHtmlView::SetBackground(const SkBitmap& background) {
  if (initialized_) {
    DCHECK(render_view_host_);
    render_view_host_->view()->SetBackground(background);
  } else {
    pending_background_ = background;
  }
}

void HWNDHtmlView::InitHidden() {
  // TODO(mpcomplete): make it possible to create a RenderView without an HWND.
  views::WidgetWin* win = new views::WidgetWin;
  win->Init(NULL, gfx::Rect(), true);
  win->SetContentsView(this);
}

void HWNDHtmlView::Init(HWND parent_hwnd) {
  DCHECK(!render_view_host_) << "Already initialized.";
  RenderViewHost* rvh = new RenderViewHost(
    site_instance_, delegate_, MSG_ROUTING_NONE, NULL);
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
  CreatingRenderer();
  rvh->CreateRenderView();
  if (!pending_background_.empty()) {
    rvh->view()->SetBackground(pending_background_);
    pending_background_.reset();
  }
  rvh->NavigateToURL(content_url_);
  initialized_ = true;
}

void HWNDHtmlView::ViewHierarchyChanged(bool is_add, View* parent,
                                        View* child) {
  if (is_add && GetWidget() && !initialized_)
    Init(GetWidget()->GetNativeView());
}
