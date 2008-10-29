// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIEWS_BOOKMARK_TABLE_VIEW_H_
#define CHROME_BROWSER_VIEWS_BOOKMARK_TABLE_VIEW_H_

#include "chrome/browser/bookmarks/bookmark_drag_data.h"
#include "chrome/views/menu.h"
#include "chrome/views/table_view.h"

class BookmarkModel;
class BookmarkNode;
class BookmarkTableModel;
class OSExchangeData;
class PrefService;
class Profile;

// A TableView implementation that shows a BookmarkTableModel.
// BookmarkTableView provides drag and drop support as well as showing a
// separate set of columns when showing search results.
class BookmarkTableView : public views::TableView {
 public:
  BookmarkTableView(Profile* profile, BookmarkTableModel* model);

  static void RegisterUserPrefs(PrefService* prefs);

  // Drag and drop methods.
  virtual bool CanDrop(const OSExchangeData& data);
  virtual void OnDragEntered(const views::DropTargetEvent& event);
  virtual int OnDragUpdated(const views::DropTargetEvent& event);
  virtual void OnDragExited();
  virtual int OnPerformDrop(const views::DropTargetEvent& event);

  // Sets the parent of the nodes being displayed. For search and recently
  // found results |parent| is NULL.
  void set_parent_node(BookmarkNode* parent) { parent_node_ = parent; }

  // Sets whether the path column should be shown. The path column is shown
  // for search results and recently bookmarked.
  void SetShowPathColumn(bool show_path_column);

  // The model as a BookmarkTableModel.
  BookmarkTableModel* bookmark_table_model() const;

  // Saves the widths of the table columns.
  void SaveColumnConfiguration();

 protected:
  // Overriden to draw a drop indicator when dropping between rows.
  virtual void PostPaint();

  // Overriden to start a drag.
  virtual LRESULT OnNotify(int w_param, LPNMHDR l_param);

 private:
  // Information used when we're the drop target of a drag and drop operation.
  struct DropInfo {
    DropInfo() : drop_index(-1), drop_operation(0), drop_on(false) {}

    BookmarkDragData drag_data;

    // Index into the table model of where the drop should occur.
    int drop_index;

    // The drop operation that should occur.
    int drop_operation;

    // Whether the drop is on drop_index or before it.
    bool drop_on;
  };

  // Starts a drop operation.
  void BeginDrag();

  // Returns the drop operation for the specified index.
  int CalculateDropOperation(const views::DropTargetEvent& event,
                             int drop_index,
                             bool drop_on);

  // Performs the drop operation.
  void OnPerformDropImpl();

  // Sets the drop index. If index differs from the current drop index
  // UpdateDropIndex is invoked for the old and new values.
  void SetDropIndex(int index, bool drop_on);

  // Invoked from SetDropIndex to update the state for the specified index
  // and schedule a paint. If |turn_on| is true the highlight is being turned
  // on for the specified index, otherwise it is being turned off.
  void UpdateDropIndex(int index, bool drop_on, bool turn_on);

  // Determines the drop index for the specified location.
  int CalculateDropIndex(int y, bool* drop_on);

  // Returns the BookmarkNode the drop should occur on, or NULL if not over
  // a valid location.
  BookmarkNode* GetDropParentAndIndex(int visual_drop_index,
                                      bool drop_on,
                                      int* index);

  // Returns the bounds of drop indicator shown when the drop is to occur
  // between rows (drop_on is false).
  RECT GetDropBetweenHighlightRect(int index);

  // Resets the columns. BookmarkTableView shows different sets of columns.
  // See ShowPathColumn for details.
  void UpdateColumns();

  Profile* profile_;

  BookmarkNode* parent_node_;

  scoped_ptr<DropInfo> drop_info_;

  bool show_path_column_;

  DISALLOW_COPY_AND_ASSIGN(BookmarkTableView);
};

#endif  // CHROME_BROWSER_VIEWS_BOOKMARK_TABLE_VIEW_H_
