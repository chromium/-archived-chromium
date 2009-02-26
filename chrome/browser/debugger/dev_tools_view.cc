// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/debugger/dev_tools_view.h"

#include "chrome/browser/browser_list.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/tab_contents/web_contents.h"
#include "chrome/browser/views/tab_contents_container_view.h"
#include "chrome/common/property_bag.h"
#include "chrome/common/render_messages.h"

DevToolsView::DevToolsView(int inspected_process_id, int inspected_view_id)
    : inspected_process_id_(inspected_process_id),
      inspected_view_id_(inspected_view_id),
      web_contents_(NULL) {
  web_container_ = new TabContentsContainerView();
  AddChildView(web_container_);
}

DevToolsView::~DevToolsView() {
}

void DevToolsView::SendDevToolsClientMessage(const IPC::Message& message) {
  if (!web_contents_) {
    NOTREACHED();
    return;
  }
  IPC::Message* copy =  new IPC::Message(message);
  copy->set_routing_id(web_contents_->render_view_host()->routing_id());
  web_contents_->render_view_host()->Send(copy);
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

  TabContents* tc = TabContents::CreateWithType(TAB_CONTENTS_DOM_UI, profile,
                                                NULL);
  web_contents_ = tc->AsWebContents();
  web_contents_->SetupController(profile);
  web_contents_->set_delegate(this);
  web_container_->SetTabContents(web_contents_);
  web_contents_->render_view_host()->AllowDOMUIBindings();
  web_contents_->render_view_host()->SetInspectedView(inspected_process_id_,
                                                      inspected_view_id_);

  GURL contents("chrome-ui://inspector/debugger-oop.html");
  // this will call CreateRenderView to create renderer process
  web_contents_->controller()->LoadURL(contents, GURL(),
                                       PageTransition::START_PAGE);
}

void DevToolsView::OnWindowClosing() {
  web_container_->SetTabContents(NULL);  // detach last (and only) tab
  web_contents_->CloseContents();  // destroy the tab and navigation controller
}

void DevToolsView::OpenURLFromTab(TabContents* source,
                               const GURL& url, const GURL& referrer,
                               WindowOpenDisposition disposition,
                               PageTransition::Type transition) {
  NOTREACHED();
}

