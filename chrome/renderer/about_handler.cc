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

#include <windows.h>

#include "chrome/renderer/about_handler.h"

#include "googleurl/src/gurl.h"

struct AboutHandlerUrl {
  char *url;
  void (*action)();
};

static AboutHandlerUrl about_urls[] = {
  { "about:crash", AboutHandler::AboutCrash },
  { "about:hang", AboutHandler::AboutHang },
  { "about:shorthang", AboutHandler::AboutShortHang },
  { NULL, NULL }
};

bool AboutHandler::WillHandle(const GURL& url) {
  if (url.scheme() != "about")
    return false;

  struct AboutHandlerUrl* url_handler = about_urls;
  while (url_handler->url) {
    if (url == GURL(url_handler->url))
      return true;
    url_handler++;
  }
  return false;
}

// static
bool AboutHandler::MaybeHandle(const GURL& url) {
  if (url.scheme() != "about")
    return false;

  struct AboutHandlerUrl* url_handler = about_urls;
  while (url_handler->url) {
    if (url == GURL(url_handler->url)) {
      url_handler->action();
      return true;  // theoretically :]
    }
    url_handler++;
  }
  return false;
}

// static
void AboutHandler::AboutCrash() {
  int *zero = NULL;
  *zero = 0;  // Null pointer dereference: kaboom!
}

// static
void AboutHandler::AboutHang() {
  for (;;) {
    Sleep(1000);
  }
}

// static
void AboutHandler::AboutShortHang() {
  Sleep(20000);
}
