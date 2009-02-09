// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/dom_view.h"

#include "chrome/browser/dom_ui/dom_ui_host.h"

DOMView::DOMView(const GURL& contents)
    : contents_(contents), initialized_(false), host_(NULL) {
}

DOMView::~DOMView() {
  if (host_) {
    Detach();
    host_->Destroy();
    host_ = NULL;
  }
}

bool DOMView::Init(Profile* profile, SiteInstance* instance) {
  if (initialized_)
    return true;
  initialized_ = true;

  // TODO(timsteele): This should use a separate factory method; e.g
  // a DOMUIHostFactory rather than TabContentsFactory, because DOMView's
  // should only be associated with instances of DOMUIHost.
  TabContentsType type = TabContents::TypeForURL(&contents_);
  TabContents* tab_contents = TabContents::CreateWithType(type, profile,
                                                          instance);
  host_ = tab_contents->AsDOMUIHost();
  DCHECK(host_);

  views::HWNDView::Attach(host_->GetNativeView());
  host_->SetupController(profile);
  host_->controller()->LoadURL(contents_, GURL(), PageTransition::START_PAGE);
  return true;
}

