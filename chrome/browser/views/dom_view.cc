// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/dom_view.h"

#include "chrome/browser/dom_ui/dom_ui_host.h"

DOMView::DOMView() : initialized_(false), web_contents_(NULL) {
  SetFocusable(true);
}

DOMView::~DOMView() {
  if (web_contents_) {
    Detach();
    web_contents_->Destroy();
    web_contents_ = NULL;
  }
}

bool DOMView::Init(Profile* profile, SiteInstance* instance) {
  if (initialized_)
    return true;

  initialized_ = true;
  web_contents_ = new WebContents(profile, instance, NULL,
                                  MSG_ROUTING_NONE, NULL);
  views::HWNDView::Attach(web_contents_->GetNativeView());
  web_contents_->SetupController(profile);
  return true;
}

void DOMView::LoadURL(const GURL& url) {
  DCHECK(initialized_);
  web_contents_->controller()->LoadURL(url, GURL(), PageTransition::START_PAGE);
}
