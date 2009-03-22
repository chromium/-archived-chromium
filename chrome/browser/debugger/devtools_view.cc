// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/debugger/devtools_view.h"

#include <string>

#include "chrome/browser/browser_list.h"
#include "chrome/browser/debugger/devtools_manager.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/tab_contents/web_contents.h"
#include "chrome/browser/views/tab_contents_container_view.h"
#include "chrome/common/property_bag.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/url_constants.h"

DevToolsView::DevToolsView(DevToolsInstanceDescriptor* descriptor)
    : descriptor_(descriptor),
      web_contents_(NULL) {
  web_container_ = new TabContentsContainerView();
  AddChildView(web_container_);
}

DevToolsView::~DevToolsView() {
}

std::string DevToolsView::GetClassName() const {
  return "DevToolsView";
}

gfx::Size DevToolsView::GetPreferredSize() {
  return gfx::Size(700, 400);
}

void DevToolsView::Layout() {
  web_container_->SetBounds(0, 0, width(), height());
}

void DevToolsView::ViewHierarchyChanged(bool is_add,
                                     views::View* parent,
                                     views::View* child) {
  if (is_add && child == this) {
    DCHECK(GetWidget());
    Init();
  }
}

void DevToolsView::Init() {
  // We can't create the WebContents until we've actually been put into a real
  // view hierarchy somewhere.
  Profile* profile = BrowserList::GetLastActive()->profile();

  TabContents* tc = TabContents::CreateWithType(TAB_CONTENTS_WEB, profile,
                                                NULL);
  web_contents_ = tc->AsWebContents();
  web_contents_->SetupController(profile);
  web_contents_->set_delegate(this);
  web_container_->SetTabContents(web_contents_);
  web_contents_->render_view_host()->AllowDOMUIBindings();
  descriptor_->SetDevToolsHost(web_contents_->render_view_host());

  // chrome-ui://devtools/devtools.html
  GURL contents(std::string(chrome::kChromeUIDevToolsURL) + "devtools.html");

  // this will call CreateRenderView to create renderer process
  web_contents_->controller()->LoadURL(contents, GURL(),
                                       PageTransition::START_PAGE);
}

void DevToolsView::OnWindowClosing() {
  DCHECK(descriptor_) << "OnWindowClosing is called twice";
  if (descriptor_) {
    descriptor_->Destroy();
    descriptor_ = NULL;
  }
  web_container_->SetTabContents(NULL);  // detach last (and only) tab
  web_contents_->CloseContents();  // destroy the tab and navigation controller
}

void DevToolsView::OpenURLFromTab(TabContents* source,
                               const GURL& url, const GURL& referrer,
                               WindowOpenDisposition disposition,
                               PageTransition::Type transition) {
  NOTREACHED();
}
