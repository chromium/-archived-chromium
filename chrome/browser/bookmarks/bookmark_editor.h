// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BOOKMARKS_BOOKMARK_EDITOR_H_
#define CHROME_BROWSER_BOOKMARKS_BOOKMARK_EDITOR_H_

#include "base/gfx/native_widget_types.h"

class BookmarkNode;
class Profile;

// Small, cross platform interface that shows the correct platform specific
// bookmark editor dialog.
class BookmarkEditor {
 public:
  // Handler is notified when the BookmarkEditor creates a new bookmark.
  // Handler is owned by the BookmarkEditor and deleted when it is deleted.
  class Handler {
   public:
    virtual ~Handler() {}
    virtual void NodeCreated(const BookmarkNode* new_node) = 0;
  };

  // An enumeration of the possible configurations offered.
  enum Configuration {
    SHOW_TREE,
    NO_TREE
  };

  // Shows the platform specific BookmarkEditor subclass editing |node|. If
  // |node| is NULL a new entry is created initially parented to |parent|. If
  // |show_tree| is false the tree is not shown. BookmarkEditor takes
  // ownership of |handler| and deletes it when done. |handler| may be
  // null. See description of Handler for details.
  static void Show(gfx::NativeView parent_window,
                   Profile* profile,
                   const BookmarkNode* parent,
                   const BookmarkNode* node,
                   Configuration configuration,
                   Handler* handler);
};

#endif  // CHROME_BROWSER_BOOKMARKS_BOOKMARK_EDITOR_H_
