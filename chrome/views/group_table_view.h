// Copyright 2008, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

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
