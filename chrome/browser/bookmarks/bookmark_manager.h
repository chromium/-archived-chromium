// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BOOKMARKS_BOOKMARK_MANAGER_H_
#define CHROME_BROWSER_BOOKMARKS_BOOKMARK_MANAGER_H_

class BookmarkNode;
class PrefService;
class Profile;

// Cross-platform API for interacting with BookmarkManager views.
// Implemented alongside the platform-specific view classes.
class BookmarkManager {
 public:
  // Select |node| in the tree view of the bookmark manager, but only if the
  // manager is showing and is for |profile|.
  static void SelectInTree(Profile* profile, const BookmarkNode* node);

  // Show the bookmark manager for |profile|. Only one bookmark manager shows
  // at a time, so if another one is showing, this does nothing.
  static void Show(Profile* profile);
};

#endif  // CHROME_BROWSER_BOOKMARKS_BOOKMARK_MANAGER_H_
