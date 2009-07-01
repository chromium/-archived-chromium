// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/debugger/devtools_client_host.h"
#include "chrome/browser/debugger/devtools_container.h"
#include "chrome/browser/debugger/devtools_manager.h"
#include "chrome/browser/debugger/devtools_window.h"
#include "chrome/browser/views/frame/browser_view.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/views/tab_contents/tab_contents_container.h"

#include "app/gfx/canvas.h"
#include "skia/ext/skia_utils_win.h"
#include "views/background.h"

DevToolsContainer::DevToolsContainer()
    : devtools_contents_(NULL) {
  contents_container_ = new TabContentsContainer();
  AddChildView(contents_container_);
}

DevToolsContainer::~DevToolsContainer() {
}

void DevToolsContainer::ChangeTabContents(TabContents* tab_contents) {
  devtools_contents_ = NULL;
  if (tab_contents) {
    DevToolsClientHost* client_host = DevToolsManager::GetInstance()->
        GetDevToolsClientHostFor(tab_contents->render_view_host());
    if (client_host) {
      DevToolsWindow* window = client_host->AsDevToolsWindow();
      if (window && window->is_docked())
        devtools_contents_ = window->tab_contents();
    }
  }
  contents_container_->ChangeTabContents(devtools_contents_);
  SetVisible(devtools_contents_ != NULL);
  GetParent()->Layout();
}

gfx::Size DevToolsContainer::GetPreferredSize() {
  return gfx::Size(800, 200);
}

void DevToolsContainer::Layout() {
  contents_container_->SetVisible(true);
  contents_container_->SetBounds(0, 0, width(), height());
  contents_container_->Layout();
}
