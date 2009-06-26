// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BOOKMARKS_BOOKMARK_TABLE_MODEL_H_
#define CHROME_BROWSER_BOOKMARKS_BOOKMARK_TABLE_MODEL_H_

#include "app/table_model.h"
#include "build/build_config.h"
#include "chrome/browser/bookmarks/bookmark_model.h"

// BookmarkTableModel provides a view of the BookmarkModel as a TableModel.
// Three variations are provided:
// . Recently created bookmarks.
// . The children of a particular folder.
// . All bookmarks matching the specified text.
class BookmarkTableModel : public TableModel,
                           public BookmarkModelObserver {
 public:
  // Methods for creating the various BookmarkTableModels. Ownership passes
  // to the caller.
  // |languages| is the kAcceptLanguages value of the user preference. It is
  // used to decode IDN.
  static BookmarkTableModel* CreateRecentlyBookmarkedModel(
      BookmarkModel* model);
  static BookmarkTableModel* CreateBookmarkTableModelForFolder(
      BookmarkModel* model, const BookmarkNode* node);
  static BookmarkTableModel* CreateSearchTableModel(
      BookmarkModel* model,
      const std::wstring& text,
      const std::wstring& languages);

  explicit BookmarkTableModel(BookmarkModel* model);
  virtual ~BookmarkTableModel();

  // TableModel methods.
  virtual std::wstring GetText(int row, int column_id);
  virtual SkBitmap GetIcon(int row);
  virtual void SetObserver(TableModelObserver* observer) {
    observer_ = observer;
  }

  // BookmarkModelObserver methods.
  virtual void Loaded(BookmarkModel* model) {}
  virtual void BookmarkModelBeingDeleted(BookmarkModel* model);
  virtual void BookmarkNodeChildrenReordered(BookmarkModel* model,
                                             const BookmarkNode* node) {}


  // Returns the index of the specified node, or -1 if the node isn't in the
  // model.
  virtual int IndexOfNode(const BookmarkNode* node);

  // Returns the BookmarkNode at the specified index.
  virtual const BookmarkNode* GetNodeForRow(int row) = 0;

  // Returns the underlying BookmarkModel.
  BookmarkModel* model() const { return model_; }

 protected:
  TableModelObserver* observer() const { return observer_; }

 private:
  // Builds the path shown in the path column for the specified node.
  void BuildPath(const BookmarkNode* node, std::wstring* path);

  BookmarkModel* model_;
  TableModelObserver* observer_;

  DISALLOW_COPY_AND_ASSIGN(BookmarkTableModel);
};

#endif  // CHROME_BROWSER_BOOKMARKS_BOOKMARK_TABLE_MODEL_H_
