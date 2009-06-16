// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef APP_TABLE_MODEL_H_
#define APP_TABLE_MODEL_H_

#include <string>
#include <vector>

#include "base/logging.h"
#include "unicode/coll.h"

class SkBitmap;

class TableModelObserver;

// The model driving the TableView.
class TableModel {
 public:
  // See HasGroups, get GetGroupID for details as to how this is used.
  struct Group {
    // The title text for the group.
    std::wstring title;

    // Unique id for the group.
    int id;
  };
  typedef std::vector<Group> Groups;

  // Number of rows in the model.
  virtual int RowCount() = 0;

  // Returns the value at a particular location in text.
  virtual std::wstring GetText(int row, int column_id) = 0;

  // Returns the small icon (16x16) that should be displayed in the first
  // column before the text. This is only used when the TableView was created
  // with the ICON_AND_TEXT table type. Returns an isNull() bitmap if there is
  // no bitmap.
  virtual SkBitmap GetIcon(int row);

  // Sets whether a particular row is checked. This is only invoked
  // if the TableView was created with show_check_in_first_column true.
  virtual void SetChecked(int row, bool is_checked) {
    NOTREACHED();
  }

  // Returns whether a particular row is checked. This is only invoked
  // if the TableView was created with show_check_in_first_column true.
  virtual bool IsChecked(int row) {
    return false;
  }

  // Returns true if the TableView has groups. Groups provide a way to visually
  // delineate the rows in a table view. When groups are enabled table view
  // shows a visual separator for each group, followed by all the rows in
  // the group.
  //
  // On win2k a visual separator is not rendered for the group headers.
  virtual bool HasGroups() { return false; }

  // Returns the groups.
  // This is only used if HasGroups returns true.
  virtual Groups GetGroups() {
    // If you override HasGroups to return true, you must override this as
    // well.
    NOTREACHED();
    return std::vector<Group>();
  }

  // Returns the group id of the specified row.
  // This is only used if HasGroups returns true.
  virtual int GetGroupID(int row) {
    // If you override HasGroups to return true, you must override this as
    // well.
    NOTREACHED();
    return 0;
  }

  // Sets the observer for the model. The TableView should NOT take ownership
  // of the observer.
  virtual void SetObserver(TableModelObserver* observer) = 0;

  // Compares the values in the column with id |column_id| for the two rows.
  // Returns a value < 0, == 0 or > 0 as to whether the first value is
  // <, == or > the second value.
  //
  // This implementation does a case insensitive locale specific string
  // comparison.
  virtual int CompareValues(int row1, int row2, int column_id);

  // Reset the collator.
  void ClearCollator();

 protected:
  // Returns the collator used by CompareValues.
  Collator* GetCollator();
};

// TableColumn specifies the title, alignment and size of a particular column.
struct TableColumn {
  enum Alignment {
    LEFT, RIGHT, CENTER
  };

  TableColumn();
  TableColumn(int id, const std::wstring& title,
              Alignment alignment, int width);
  TableColumn(int id, const std::wstring& title,
              Alignment alignment, int width, float percent);

  // It's common (but not required) to use the title's IDS_* tag as the column
  // id. In this case, the provided conveniences look up the title string on
  // bahalf of the caller.
  TableColumn(int id, Alignment alignment, int width);
  TableColumn(int id, Alignment alignment, int width, float percent);

  // A unique identifier for the column.
  int id;

  // The title for the column.
  std::wstring title;

  // Alignment for the content.
  Alignment alignment;

  // The size of a column may be specified in two ways:
  // 1. A fixed width. Set the width field to a positive number and the
  //    column will be given that width, in pixels.
  // 2. As a percentage of the available width. If width is -1, and percent is
  //    > 0, the column is given a width of
  //    available_width * percent / total_percent.
  // 3. If the width == -1 and percent == 0, the column is autosized based on
  //    the width of the column header text.
  //
  // Sizing is done in four passes. Fixed width columns are given
  // their width, percentages are applied, autosized columns are autosized,
  // and finally percentages are applied again taking into account the widths
  // of autosized columns.
  int width;
  float percent;

  // The minimum width required for all items in this column
  // (including the header)
  // to be visible.
  int min_visible_width;

  // Is this column sortable? Default is false
  bool sortable;
};

#endif  // APP_TABLE_MODEL_H_
