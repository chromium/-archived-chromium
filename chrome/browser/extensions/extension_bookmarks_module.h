// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_BOOKMARKS_MODULE_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_BOOKMARKS_MODULE_H_

#include <string>
#include <vector>

#include "base/singleton.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/extensions/extension_function.h"
#include "chrome/browser/extensions/extension_tabs_module.h"

// Observes BookmarkModel and then routes the notifications as events to
// the extension system.
class ExtensionBookmarkEventRouter : public BookmarkModelObserver {
 public:
  static ExtensionBookmarkEventRouter* GetSingleton();
  virtual ~ExtensionBookmarkEventRouter();

  // Call this for each model to observe.  Safe to call multiple times per
  // model.
  void Observe(BookmarkModel* model);

  // BookmarkModelObserver
  virtual void Loaded(BookmarkModel* model);
  virtual void BookmarkModelBeingDeleted(BookmarkModel* model) { }
  virtual void BookmarkNodeMoved(BookmarkModel* model,
                                 BookmarkNode* old_parent,
                                 int old_index,
                                 BookmarkNode* new_parent,
                                 int new_index);
  virtual void BookmarkNodeAdded(BookmarkModel* model,
                                 BookmarkNode* parent,
                                 int index);
  virtual void BookmarkNodeRemoved(BookmarkModel* model,
                                   BookmarkNode* parent,
                                   int index);
  virtual void BookmarkNodeRemoved(BookmarkModel* model,
                                   BookmarkNode* parent,
                                   int old_index,
                                   BookmarkNode* node) {
    BookmarkNodeRemoved(model, parent, old_index);
  }
  virtual void BookmarkNodeChanged(BookmarkModel* model,
                                   BookmarkNode* node);
  virtual void BookmarkNodeFavIconLoaded(BookmarkModel* model,
                                         BookmarkNode* node);
  virtual void BookmarkNodeChildrenReordered(BookmarkModel* model,
                                             BookmarkNode* node);

 private:
  ExtensionBookmarkEventRouter();
  friend struct DefaultSingletonTraits<ExtensionBookmarkEventRouter>;

  // Helper to actually dispatch an event to extension listeners.
  void DispatchEvent(Profile* profile,
                     const char* event_name,
                     const std::string json_args);

  // These are stored so that Observe can be called multiple times safely.
  // This way the caller doesn't have to know whether it's already observing
  // a particular model or not.  The pointers are not owned by this object.
  std::set<BookmarkModel*> models_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionBookmarkEventRouter);
};

class BookmarksFunction : public SyncExtensionFunction {
  virtual void Run();
};

class GetBookmarksFunction : public BookmarksFunction {
  virtual bool RunImpl();
};

class GetBookmarkChildrenFunction : public BookmarksFunction {
  virtual bool RunImpl();
};

class GetBookmarkTreeFunction : public BookmarksFunction {
  virtual bool RunImpl();
};

class SearchBookmarksFunction : public BookmarksFunction {
  virtual bool RunImpl();
};

class RemoveBookmarkFunction : public BookmarksFunction {
  virtual bool RunImpl();
};

class CreateBookmarkFunction : public BookmarksFunction {
  virtual bool RunImpl();
};

class MoveBookmarkFunction : public BookmarksFunction {
  virtual bool RunImpl();
};

class SetBookmarkTitleFunction : public BookmarksFunction {
  virtual bool RunImpl();
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_BOOKMARKS_MODULE_H_
