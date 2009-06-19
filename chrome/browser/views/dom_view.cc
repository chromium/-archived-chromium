// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/dom_view.h"

#include "chrome/browser/tab_contents/tab_contents.h"
#include "views/focus/focus_manager.h"

DOMView::DOMView() : initialized_(false), tab_contents_(NULL) {
  SetFocusable(true);
}

DOMView::~DOMView() {
  if (tab_contents_.get())
    Detach();
}

bool DOMView::Init(Profile* profile, SiteInstance* instance) {
  if (initialized_)
    return true;

  initialized_ = true;
  tab_contents_.reset(new TabContents(profile, instance,
                                      MSG_ROUTING_NONE, NULL));
  views::NativeViewHost::Attach(tab_contents_->GetNativeView());
  return true;
}

void DOMView::LoadURL(const GURL& url) {
  DCHECK(initialized_);
  tab_contents_->controller().LoadURL(url, GURL(), PageTransition::START_PAGE);
}

bool DOMView::SkipDefaultKeyEventProcessing(const views::KeyEvent& e) {
  // Don't move the focus to the next view when tab is pressed, we want the
  // key event to be propagated to the render view for doing the tab traversal
  // there.
  return views::FocusManager::IsTabTraversalKeyEvent(e);
}

void DOMView::Focus() {
  tab_contents_->Focus();
}
