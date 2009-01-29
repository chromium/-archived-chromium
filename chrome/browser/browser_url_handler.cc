// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browser_url_handler.h"

#include "chrome/browser/browser_about_handler.h"
#include "chrome/browser/dom_ui/new_tab_ui.h"
#include "chrome/browser/dom_ui/dom_ui_contents.h"

std::vector<BrowserURLHandler::URLHandler> BrowserURLHandler::url_handlers_;

// static
void BrowserURLHandler::InitURLHandlers() {
  if (!url_handlers_.empty())
    return;

  // Here is where we initialize the global list of handlers for special URLs.
  // about:*
  url_handlers_.push_back(&BrowserAboutHandler::MaybeHandle);
  // chrome-internal:*
  url_handlers_.push_back(&NewTabUIHandleURL);
  // chrome-ui:*
  url_handlers_.push_back(&DOMUIContentsCanHandleURL);
}

// static
bool BrowserURLHandler::HandleBrowserURL(GURL* url, TabContentsType* type) {
  if (url_handlers_.empty())
    InitURLHandlers();
  for (size_t i = 0; i < url_handlers_.size(); ++i) {
    if ((*url_handlers_[i])(url, type))
      return true;
  }
  return false;
}


