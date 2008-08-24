// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_VIEWS_GROUP_TABLE_VIEW_H__
#define CHROME_VIEWS_GROUP_TABLE_VIEW_H__

#include "base/task.h"
#include "chrome/views/table_view.h"

// The GroupTableView adds grouping to the TableView class.
// It allows to have groups of rows that act as a single row from the selection
// perspective. Groups are visually separated by a horizontal line.

namespace ChromeViews {

struct GroupRange {
  int start;
  int length;
};

// The model driving the GroupTableView.
class GroupTableModel : public TableModel {
 public:
  // Populates the passed range with the first row/last row (included)
  // that this item belongs to.
  virtual void GetGroupRangeForItem(int item, GroupRange* range) = 0;
};

class GroupTableView : public TableView {
 public:
   // The view class name.
   static const char kViewClassName[];

  GroupTableView(GroupTableModel* model,
                 const std::vector<TableColumn>& columns,
                 TableTypes table_type, bool single_selection,
                 bool resizable_columns, bool autosize_columns);
  virtual ~GroupTableView();

  virtual std::string GetClassName() const;

 protected:
  // Notification from the ListView that the selected state of an item has
  // changed.
  void OnSelectedStateChanged(int item, bool is_selected);

  // Extra-painting required to draw the separator line between groups.
  virtual bool ImplementPostPaint() { return true; }
  virtual void PostPaint(int row, int column, bool selected,
                         const CRect& bounds, HDC device_context);

  // In order to make keyboard navigation possible (using the Up and Down
  // keys), we must take action when an arrow key is pressed. The reason we
  // need to process this message has to do with the manner in which the focus
  // needs to be set on a group item when a group is selected.
  virtual void OnKeyDown(unsigned short virtual_keycode);

 private:
  // Make the selection of group consistent.
  void SyncSelection();

  GroupTableModel *model_;

  // A factory to make the selection consistent among groups.
  ScopedRunnableMethodFactory<GroupTableView> sync_selection_factory_;

  DISALLOW_EVIL_CONSTRUCTORS(GroupTableView);
};

}  // namespace ChromeViews

#endif  // CHROME_VIEWS_GROUP_TABLE_VIEW_H__

