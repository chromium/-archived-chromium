// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TAB_CONTENTS_TAB_CONTENTS_TYPE_H__
#define CHROME_BROWSER_TAB_CONTENTS_TAB_CONTENTS_TYPE_H__

// The different kinds of tab contents we support. This is declared outside of
// TabContents to eliminate the circular dependency between NavigationEntry
// (which requires a tab type) and TabContents (which requires a
// NavigationEntry).
enum TabContentsType {
  TAB_CONTENTS_UNKNOWN_TYPE = 0,
  TAB_CONTENTS_WEB,
  TAB_CONTENTS_DOWNLOAD_VIEW,
  TAB_CONTENTS_CHROME_VIEW_CONTENTS,
  TAB_CONTENTS_NEW_TAB_UI,
  TAB_CONTENTS_HTML_DIALOG,
  TAB_CONTENTS_ABOUT_UI,
  TAB_CONTENTS_DEBUGGER,
  TAB_CONTENTS_DOM_UI,
  TAB_CONTENTS_NUM_TYPES
};

#endif  // CHROME_BROWSER_TAB_CONTENTS_TAB_CONTENTS_TYPE_H__

