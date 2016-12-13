// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// A struct for managing browser's settings that apply to the renderer or its
// webview.  These differ from WebPreferences since they apply to Chromium's
// glue layer rather than applying to just WebKit.
//
// Adding new values to this class probably involves updating
// common/render_messages.h, browser/browser.cc, etc.

#ifndef CHROME_COMMON_RENDERER_PREFERENCES_H_
#define CHROME_COMMON_RENDERER_PREFERENCES_H_

struct RendererPreferences {
  // Whether the renderer's current browser context accept drops from the OS
  // that result in navigations away from the current page.
  bool can_accept_load_drops;

  RendererPreferences()
      : can_accept_load_drops(true) {
  }
};

#endif  // CHROME_COMMON_RENDERER_PREFERENCES_H_
