// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_OPTIONS_WINDOW_H__
#define CHROME_BROWSER_OPTIONS_WINDOW_H__

class Profile;

// An identifier for a Options Tab page. These are treated as indices into
// the list of available tabs to be displayed. PAGE_DEFAULT means select the
// last tab viewed when the Options window was opened, or PAGE_GENERAL if the
// Options was never opened.
enum OptionsPage {
  OPTIONS_PAGE_DEFAULT = -1,
  OPTIONS_PAGE_GENERAL,
  OPTIONS_PAGE_CONTENT,
#ifdef CHROME_PERSONALIZATION
  OPTIONS_PAGE_USER_DATA,
#endif
  OPTIONS_PAGE_ADVANCED,
  OPTIONS_PAGE_COUNT
};

// These are some well known groups within the Options dialog box that we may
// wish to highlight to attract the user's attention to.
enum OptionsGroup {
  OPTIONS_GROUP_NONE,
  OPTIONS_GROUP_DEFAULT_SEARCH
};

// Show the Options window selecting the specified page. If an Options window
// is currently open, this just activates it instead of opening a new one.
void ShowOptionsWindow(OptionsPage page,
                       OptionsGroup highlight_group,
                       Profile* profile);

#endif  // #ifndef CHROME_BROWSER_OPTIONS_WINDOW_H__
