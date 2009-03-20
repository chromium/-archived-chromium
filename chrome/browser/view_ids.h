// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This defines an enumeration of IDs that can uniquely identify a view within
// the scope of a container view.

#ifndef CHROME_BROWSER_VIEW_IDS_H_
#define CHROME_BROWSER_VIEW_IDS_H_

enum ViewID {
  VIEW_ID_NONE = 0,

  // BROWSER WINDOW VIEWS
  // ------------------------------------------------------

  // Tabs within a window/tab strip, counting from the left.
  VIEW_ID_TAB_0,
  VIEW_ID_TAB_1,
  VIEW_ID_TAB_2,
  VIEW_ID_TAB_3,
  VIEW_ID_TAB_4,
  VIEW_ID_TAB_5,
  VIEW_ID_TAB_6,
  VIEW_ID_TAB_7,
  VIEW_ID_TAB_8,
  VIEW_ID_TAB_9,
  VIEW_ID_TAB_LAST,

  // Toolbar & toolbar elements.
  VIEW_ID_TOOLBAR = 1000,
  VIEW_ID_BACK_BUTTON,
  VIEW_ID_FORWARD_BUTTON,
  VIEW_ID_RELOAD_BUTTON,
  VIEW_ID_HOME_BUTTON,
  VIEW_ID_STAR_BUTTON,
  VIEW_ID_LOCATION_BAR,
  VIEW_ID_GO_BUTTON,
  VIEW_ID_PAGE_MENU,
  VIEW_ID_APP_MENU,
  VIEW_ID_AUTOCOMPLETE,
  VIEW_ID_BOOKMARK_MENU,

  // The Bookmark Bar.
  VIEW_ID_BOOKMARK_BAR,

  // Find in page.
  VIEW_ID_FIND_IN_PAGE_TEXT_FIELD,

  // Tab Container window.
  VIEW_ID_TAB_CONTAINER,

  VIEW_ID_PREDEFINED_COUNT
};

#endif  // CHROME_BROWSER_VIEW_IDS_H_
