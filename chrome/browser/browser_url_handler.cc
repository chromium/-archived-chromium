// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browser_url_handler.h"

#include "base/string_util.h"
#include "chrome/browser/browser_about_handler.h"
#include "chrome/browser/dom_ui/dom_ui_factory.h"
#include "chrome/common/url_constants.h"
#include "googleurl/src/gurl.h"

// Handles rewriting view-source URLs for what we'll actually load.
static bool HandleViewSource(GURL* url) {
  if (url->SchemeIs(chrome::kViewSourceScheme)) {
    // Load the inner URL instead.
    *url = GURL(url->path());
    return true;
  }
  return false;
}

// Handles URLs for DOM UI. These URLs need no rewriting.
static bool HandleDOMUI(GURL* url) {
  if (!DOMUIFactory::UseDOMUIForURL(*url))
    return false;
  return true;
}

std::vector<BrowserURLHandler::URLHandler> BrowserURLHandler::url_handlers_;

// static
void BrowserURLHandler::InitURLHandlers() {
  if (!url_handlers_.empty())
    return;

  // Add the default URL handlers.
  url_handlers_.push_back(&WillHandleBrowserAboutURL);  // about:
  url_handlers_.push_back(&HandleDOMUI);                // chrome: & friends.
  url_handlers_.push_back(&HandleViewSource);           // view-source:
}

// static
void BrowserURLHandler::RewriteURLIfNecessary(GURL* url) {
  if (url_handlers_.empty())
    InitURLHandlers();
  for (size_t i = 0; i < url_handlers_.size(); ++i) {
    if ((*url_handlers_[i])(url))
      return;
  }
}
