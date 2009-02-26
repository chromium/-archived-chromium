// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/about_handler.h"

#include "base/platform_thread.h"
#include "chrome/common/url_constants.h"
#include "googleurl/src/gurl.h"

struct AboutHandlerUrl {
  const char *url;
  void (*action)();
};

static AboutHandlerUrl about_urls[] = {
  { chrome::kAboutCrashURL, AboutHandler::AboutCrash },
  { chrome::kAboutHangURL, AboutHandler::AboutHang },
  { chrome::kAboutShortHangURL, AboutHandler::AboutShortHang },
  { NULL, NULL }
};

bool AboutHandler::WillHandle(const GURL& url) {
  if (url.SchemeIs(chrome::kAboutScheme))
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
  if (!url.SchemeIs(chrome::kAboutScheme))
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
    PlatformThread::Sleep(1000);
  }
}

// static
void AboutHandler::AboutShortHang() {
  PlatformThread::Sleep(20000);
}

