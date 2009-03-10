// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// We handle some special browser-level URLs (like "about:version")
// before they're handed to a renderer.  This lets us do the URL handling
// on the browser side (which has access to more information than the
// renderers do) as well as sidestep the risk of exposing data to
// random web pages (because from the resource loader's perspective, these
// URL schemes don't exist).

#ifndef CHROME_BROWSER_BROWSER_URL_HANDLER_H_
#define CHROME_BROWSER_BROWSER_URL_HANDLER_H_

#include <vector>

#include "chrome/browser/tab_contents/tab_contents_type.h"

class GURL;

// BrowserURLHandler manages the list of all special URLs and manages
// dispatching the URL handling to registered handlers.
class BrowserURLHandler {
 public:
  // The type of functions that can process a URL.
  // If a handler handles |url|, it should :
  // - optionally modify |url| to the URL that should be sent to the renderer
  // - set |type| to the proper TabContentsType
  // - optionally set |dispatcher| to the necessary DOMMessageDispatcher
  // - return true.
  // If the URL is not handled by a handler, it should return false.
  typedef bool (*URLHandler)(GURL* url, TabContentsType* type);

  // HandleBrowserURL gives all registered URLHandlers a shot at processing
  // this URL, and returns true if one of them handled the URL.  If MaybeHandle
  // returns false, then |type| and |dispatcher| should not be used and
  // the URL should be handled like any normal URL.
  static bool HandleBrowserURL(GURL* url, TabContentsType* type);

  // We initialize the list of url_handlers_ lazily the first time MaybeHandle
  // is called.
  static void InitURLHandlers();

  // The list of known URLHandlers.
  static std::vector<URLHandler> url_handlers_;
};

#endif  // CHROME_BROWSER_BROWSER_URL_HANDLER_H_
