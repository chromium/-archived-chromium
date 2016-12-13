// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Contains code for handling "about:" URLs in the browser process.

#ifndef CHROME_BROWSER_BROWSER_ABOUT_HANDLER_H_
#define CHROME_BROWSER_BROWSER_ABOUT_HANDLER_H_

class GURL;

// Decides whether the given URL will be handled by the browser about handler
// and returns true if so. On true, it may also modify the given URL to be the
// final form (we fix up most "about:" URLs to be "chrome:" because WebKit
// handles all "about:" URLs as "about:blank.
//
// This is used by BrowserURLHandler.
bool WillHandleBrowserAboutURL(GURL* url);

// We have a few magic commands that don't cause navigations, but rather pop up
// dialogs. This function handles those cases, and returns true if so. In this
// case, normal tab navigation should be skipped.
bool HandleNonNavigationAboutURL(const GURL& url);

#endif  // CHROME_BROWSER_BROWSER_ABOUT_HANDLER_H_
