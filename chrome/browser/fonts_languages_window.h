// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FONTS_LANGUAGES_WINDOW_H_
#define CHROME_BROWSER_FONTS_LANGUAGES_WINDOW_H_

#include "base/gfx/native_widget_types.h"

class Profile;

// An identifier for Fonts and Languages page. These are treated as indices into
// the list of available tabs to be displayed.
enum FontsLanguagesPage {
  FONTS_ENCODING_PAGE = 0,
  LANGUAGES_PAGE
};

// Show the Fonts and Languages window selecting the specified page. If a
// Fonts and Languages window is currently open, this just activates it instead
// of opening a new one.
void ShowFontsLanguagesWindow(gfx::NativeWindow window,
                              FontsLanguagesPage page,
                              Profile* profile);

#endif  // CHROME_BROWSER_FONTS_LANGUAGES_WINDOW_H_
