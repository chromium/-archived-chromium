// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BOOKMARKS_BASE_BOOKMARK_MODEL_OBSERVER_H_
#define CHROME_BROWSER_BOOKMARKS_BASE_BOOKMARK_MODEL_OBSERVER_H_

#include "chrome/browser/bookmarks/bookmark_model.h"

// Base class for a BookmarkModelObserver implementation. All mutations of the
// model funnel into the method BookmarkModelChanged.
class BaseBookmarkModelObserver : public BookmarkModelObserver {
 public:
  BaseBookmarkModelObserver() {}
  virtual ~BaseBookmarkModelObserver() {}

  virtual void BookmarkModelChanged() = 0;

  virtual void Loaded(BookmarkModel* model) {}

  virtual void BookmarkModelBeingDeleted(BookmarkModel* model) {
    BookmarkModelChanged();
  }
  virtual void BookmarkNodeMoved(BookmarkModel* model,
                                 const BookmarkNode* old_parent,
                                 int old_index,
                                 const BookmarkNode* new_parent,
                                 int new_index) {
    BookmarkModelChanged();
  }
  virtual void BookmarkNodeAdded(BookmarkModel* model,
                                 const BookmarkNode* parent,
                                 int index) {
    BookmarkModelChanged();
  }
  virtual void BookmarkNodeRemoved(BookmarkModel* model,
                                   const BookmarkNode* parent,
                                   int index) {
    BookmarkModelChanged();
  }
  virtual void BookmarkNodeChanged(BookmarkModel* model,
                                   const BookmarkNode* node) {
    BookmarkModelChanged();
  }
  virtual void BookmarkNodeFavIconLoaded(BookmarkModel* model,
                                         const BookmarkNode* node) {
  }
  virtual void BookmarkNodeChildrenReordered(BookmarkModel* model,
                                             const BookmarkNode* node) {
    BookmarkModelChanged();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(BaseBookmarkModelObserver);
};

#endif  // CHROME_BROWSER_BOOKMARKS_BASE_BOOKMARK_MODEL_OBSERVER_H_
