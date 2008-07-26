// Copyright 2008, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

// We handle some special browser-level URLs (like "about:version")
// before they're handed to a renderer.  This lets us do the URL handling
// on the browser side (which has access to more information than the
// renderers do) as well as sidestep the risk of exposing data to
// random web pages (because from the resource loader's perspective, these
// URL schemes don't exist).

#ifndef CHROME_BROWSER_BROWSER_URL_HANDLER_H__
#define CHROME_BROWSER_BROWSER_URL_HANDLER_H__

#include <vector>

class GURL;
enum TabContentsType;

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

#endif  // CHROME_BROWSER_BROWSER_URL_HANDLER_H__
