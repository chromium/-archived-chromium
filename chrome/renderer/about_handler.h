// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Contains code for handling "about:" URLs in the renderer process.  We handle
// most about: URLs in the browser process (see
// browser/browser_about_handler.*), but test URLs like about:crash need to
// happen in the renderer.

#ifndef CHROME_RENDERER_ABOUT_HANDLER_H__
#define CHROME_RENDERER_ABOUT_HANDLER_H__

#include "base/basictypes.h"

class GURL;

class AboutHandler {
 public:
  // Given a URL, determine whether or not to handle it specially.  Returns
  // true if the URL was handled.
  static bool MaybeHandle(const GURL& url);

  // Returns true if the URL is one that this AboutHandler will handle when
  // MaybeHandle is called.
  static bool WillHandle(const GURL& url);

  // Induces a renderer crash.
  static void AboutCrash();

  // Induces a renderer hang.
  static void AboutHang();

  // Induces a brief (20 second) hang to make sure hang monitors go away.
  static void AboutShortHang();

 private:
  AboutHandler();
  ~AboutHandler();

  DISALLOW_EVIL_CONSTRUCTORS(AboutHandler);
};

#endif  // CHROME_RENDERER_ABOUT_HANDLER_H__
