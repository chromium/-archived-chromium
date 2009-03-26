// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dom_ui/dom_ui_factory.h"

#include "chrome/browser/dom_ui/downloads_ui.h"
#include "chrome/browser/dom_ui/debugger_ui.h"
#include "chrome/browser/dom_ui/devtools_ui.h"
#include "chrome/browser/dom_ui/history_ui.h"
#include "chrome/browser/dom_ui/new_tab_ui.h"
#include "chrome/browser/extensions/extensions_ui.h"
#include "chrome/common/url_constants.h"
#include "googleurl/src/gurl.h"

// Backend for both querying for and creating new DOMUI objects. If you're just
// querying whether there's a DOM UI for the given URL, pass NULL for both the
// web contents and the new_ui. The return value will indiacate whether a DOM UI
// exists for the given URL.
//
// If you want to create a DOM UI, pass non-NULL pointers for both web_contents
// and new_ui. The *new_ui pointer will be filled with the created UI if it
// succeeds (indicated by a return value of true). The caller owns the *new_ui
// pointer.
static bool CreateDOMUI(const GURL& url, WebContents* web_contents,
                        DOMUI** new_ui) {
  // This will get called a lot to check all URLs, so do a quick check of the
  // scheme to filter out most URLs.
  if (!url.SchemeIs(chrome::kChromeInternalScheme) &&
      !url.SchemeIs(chrome::kChromeUIScheme))
    return false;

  // Special case the new tab page. In older versions of Chrome, the new tab
  // page was hosted at chrome-internal:<blah>. This might be in people's saved
  // sessions or bookmarks, so we say any URL with that scheme triggers the new
  // tab page.
  if (url.host() == chrome::kChromeUINewTabHost ||
      url.SchemeIs(chrome::kChromeInternalScheme)) {
    if (new_ui)
      *new_ui = new NewTabUI(web_contents);
    return true;
  }

  // We must compare hosts only since some of the DOM UIs append extra stuff
  // after the host name.
  if (url.host() == chrome::kChromeUIHistoryHost) {
    if (new_ui)
      *new_ui = new HistoryUI(web_contents);
    return true;
  }

  if (url.host() == chrome::kChromeUIDownloadsHost) {
    if (new_ui)
      *new_ui = new DownloadsUI(web_contents);
    return true;
  }

  if (url.host() == chrome::kChromeUIExtensionsHost) {
    if (new_ui)
      *new_ui = new ExtensionsUI(web_contents);
    return true;
  }

  if (url.host() == chrome::kChromeUIInspectorHost) {
    if (new_ui)
      *new_ui = new DebuggerUI(web_contents);
    return true;
  }

  if (url.host() == chrome::kChromeUIDevToolsHost) {
    if (new_ui)
      *new_ui = new DevToolsUI(web_contents);
    return true;
  }

  return false;
}

// static
bool DOMUIFactory::HasDOMUIScheme(const GURL& url) {
  return url.SchemeIs(chrome::kChromeInternalScheme) ||
         url.SchemeIs(chrome::kChromeUIScheme);
}

// static
bool DOMUIFactory::UseDOMUIForURL(const GURL& url) {
  return CreateDOMUI(url, NULL, NULL);
}

// static
DOMUI* DOMUIFactory::CreateDOMUIForURL(WebContents* web_contents,
                                       const GURL& url) {
  DOMUI* dom_ui;
  if (!CreateDOMUI(url, web_contents, &dom_ui))
    return NULL;
  return dom_ui;
}
