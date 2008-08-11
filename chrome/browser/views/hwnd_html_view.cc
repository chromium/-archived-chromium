// Copyright 2008, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "chrome/browser/views/hwnd_html_view.h"

#include "chrome/browser/render_view_host.h"
#include "chrome/browser/render_widget_host_hwnd.h"
#include "chrome/browser/render_view_host_delegate.h"
#include "chrome/browser/site_instance.h"
#include "chrome/views/view_container.h"

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

  RenderWidgetHostHWND* view = new RenderWidgetHostHWND(rvh);
  rvh->set_view(view);

  // Create the HWND. Note:
  // RenderWidgetHostHWND supports windowed plugins, but if we ever also wanted
  // to support constrained windows with this, we would need an additional
  // HWND to parent off of because windowed plugin HWNDs cannot exist in the
  // same z-order as constrained windows.
  HWND hwnd = view->Create(parent_hwnd);
  view->ShowWindow(SW_SHOW);
  ChromeViews::HWNDView::Attach(hwnd);

  // Start up the renderer.
  if (allow_dom_ui_bindings_)
    rvh->AllowDOMUIBindings();
  rvh->CreateRenderView();
  rvh->NavigateToURL(content_url_);
  initialized_ = true;
}

void HWNDHtmlView::ViewHierarchyChanged(bool is_add, View* parent,
                                        View* child) {
  if (is_add && GetViewContainer() && !initialized_)
    Init(GetViewContainer()->GetHWND());
}