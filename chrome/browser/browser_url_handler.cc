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

#include "chrome/browser/browser_url_handler.h"

#include "chrome/browser/browser_about_handler.h"
#include "chrome/browser/dom_ui/new_tab_ui.h"

std::vector<BrowserURLHandler::URLHandler> BrowserURLHandler::url_handlers_;

// static
void BrowserURLHandler::InitURLHandlers() {
  if (!url_handlers_.empty())
    return;

  // Here is where we initialize the global list of handlers for special URLs.
  // about:*
  url_handlers_.push_back(&BrowserAboutHandler::MaybeHandle);
  // chrome:*
  url_handlers_.push_back(&NewTabUIHandleURL);
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

