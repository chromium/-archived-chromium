// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "app/table_model.h"
#include "app/table_model_observer.h"
#include "base/message_loop.h"
#include "base/string_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "views/controls/table/table_view.h"
#include "views/window/window_delegate.h"
#include "views/window/window_win.h"

using views::TableView;

// TestTableModel --------------------------------------------------------------

// Trivial TableModel implementation that is backed by a vector of vectors.
// Provides methods for adding/removing/changing the contents that notify the
// observer appropriately.
//
// Initial contents are:
// 0, 1
// 1, 1
// 2, 2
class TestTableModel : public TableModel {
 public:
  TestTableModel();

  // Adds a new row at index |row| with values |c1_value| and |c2_value|.
  void AddRow(int row, int c1_value, int c2_value);

  // Removes the row at index |row|.
  void RemoveRow(int row);

  // Changes the values of the row at |row|.
  void ChangeRow(int row, int c1_value, int c2_value);

  // TableModel
  virtual int RowCount();
  virtual std::wstring GetText(int row, int column_id);
  virtual void SetObserver(TableModelObserver* observer);
  virtual int CompareValues(int row1, int row2, int column_id);

 private:
  TableModelObserver* observer_;

  // The data.
  std::vector<std::vector<int>> rows_;

  DISALLOW_COPY_AND_ASSIGN(TestTableModel);
};

TestTableModel::TestTableModel() : observer_(NULL) {
  AddRow(0, 0, 1);
  AddRow(1, 1, 1);
  AddRow(2, 2, 2);
}

void TestTableModel::AddRow(int row, int c1_value, int c2_value) {
  DCHECK(row >= 0 && row <= static_cast<int>(rows_.size()));
  std::vector<int> new_row;
  new_row.push_back(c1_value);
  new_row.push_back(c2_value);
  rows_.insert(rows_.begin() + row, new_row);
  if (observer_)
    observer_->OnItemsAdded(row, 1);
}
void TestTableModel::RemoveRow(int row) {
  DCHECK(row >= 0 && row <= static_cast<int>(rows_.size()));
  rows_.erase(rows_.begin() + row);
  if (observer_)
    observer_->OnItemsRemoved(row, 1);
}

void TestTableModel::ChangeRow(int row, int c1_value, int c2_value) {
  DCHECK(row >= 0 && row < static_cast<int>(rows_.size()));
  rows_[row][0] = c1_value;
  rows_[row][1] = c2_value;
  if (observer_)
    observer_->OnItemsChanged(row, 1);
}

int TestTableModel::RowCount() {
  return static_cast<int>(rows_.size());
}

std::wstring TestTableModel::GetText(int row, int column_id) {
  return IntToWString(rows_[row][column_id]);
}

void TestTableModel::SetObserver(TableModelObserver* observer) {
  observer_ = observer;
}

int TestTableModel::CompareValues(int row1, int row2, int column_id) {
  return rows_[row1][column_id] - rows_[row2][column_id];
}

// TableViewTest ---------------------------------------------------------------

class TableViewTest : public testing::Test, views::WindowDelegate {
 public:
  virtual void SetUp();
  virtual void TearDown();

  virtual views::View* GetContentsView() {
    return table_;
  }

 protected:
  // Creates the model.
  TestTableModel* CreateModel();

  // Verifies the view order matches that of the supplied arguments. The
  // arguments are in terms of the model. For example, values of '1, 0' indicate
  // the model index at row 0 is 1 and the model index at row 1 is 0.
  void VeriyViewOrder(int first, ...);

  // Verifies the selection matches the supplied arguments. The supplied
  // arguments are in terms of this model. This uses the iterator returned by
  // SelectionBegin.
  void VerifySelectedRows(int first, ...);

  // Configures the state for the various multi-selection tests.
  // This selects model rows 0 and 1, and if |sort| is true the first column
  // is sorted in descending order.
  void SetUpMultiSelectTestState(bool sort);

  scoped_ptr<TestTableModel> model_;

  // The table. This is owned by the window.
  TableView* table_;

 private:
  MessageLoopForUI message_loop_;
  views::Window* window_;
};

void TableViewTest::SetUp() {
  OleInitialize(NULL);
  model_.reset(CreateModel());
  std::vector<TableColumn> columns;
  columns.resize(2);
  columns[0].id = 0;
  columns[1].id = 1;
  table_ = new TableView(model_.get(), columns, views::ICON_AND_TEXT,
                         false, false, false);
  window_ =
      views::Window::CreateChromeWindow(NULL,
                                              gfx::Rect(100, 100, 512, 512),
                                              this);
}

void TableViewTest::TearDown() {
  window_->Close();
  // Temporary workaround to avoid leak of RootView::pending_paint_task_.
  message_loop_.RunAllPending();
  OleUninitialize();
}

void TableViewTest::VeriyViewOrder(int first, ...) {
  va_list marker;
  va_start(marker, first);
  int value = first;
  int index = 0;
  for (int value = first, index = 0; value != -1; index++) {
    ASSERT_EQ(value, table_->view_to_model(index));
    value = va_arg(marker, int);
  }
  va_end(marker);
}

void TableViewTest::VerifySelectedRows(int first, ...) {
  va_list marker;
  va_start(marker, first);
  int value = first;
  int index = 0;
  TableView::iterator selection_iterator = table_->SelectionBegin();
  for (int value = first, index = 0; value != -1; index++) {
    ASSERT_TRUE(selection_iterator != table_->SelectionEnd());
    ASSERT_EQ(value, *selection_iterator);
    value = va_arg(marker, int);
    ++selection_iterator;
  }
  ASSERT_TRUE(selection_iterator == table_->SelectionEnd());
  va_end(marker);
}

void TableViewTest::SetUpMultiSelectTestState(bool sort) {
  // Select two rows.
  table_->SetSelectedState(0, true);
  table_->SetSelectedState(1, true);

  VerifySelectedRows(1, 0, -1);
  if (!sort || HasFatalFailure())
    return;

  // Sort by first column descending.
  TableView::SortDescriptors sd;
  sd.push_back(TableView::SortDescriptor(0, false));
  table_->SetSortDescriptors(sd);
  VeriyViewOrder(2, 1, 0, -1);
  if (HasFatalFailure())
    return;

  // Make sure the two rows are sorted.
  // NOTE: the order changed because iteration happens over view indices.
  VerifySelectedRows(0, 1, -1);
}

TestTableModel* TableViewTest::CreateModel() {
  return new TestTableModel();
}

// NullModelTableViewTest ------------------------------------------------------

class NullModelTableViewTest : public TableViewTest {
 protected:
  // Creates the model.
  TestTableModel* CreateModel() {
    return NULL;
  }
};

// Tests -----------------------------------------------------------------------

// Tests various sorting permutations.
TEST_F(TableViewTest, Sort) {
  // Sort by first column descending.
  TableView::SortDescriptors sort;
  sort.push_back(TableView::SortDescriptor(0, false));
  table_->SetSortDescriptors(sort);
  VeriyViewOrder(2, 1, 0, -1);
  if (HasFatalFailure())
    return;

  // Sort by second column ascending, first column descending.
  sort.clear();
  sort.push_back(TableView::SortDescriptor(1, true));
  sort.push_back(TableView::SortDescriptor(0, false));
  sort[1].ascending = false;
  table_->SetSortDescriptors(sort);
  VeriyViewOrder(1, 0, 2, -1);
  if (HasFatalFailure())
    return;

  // Clear the sort.
  table_->SetSortDescriptors(TableView::SortDescriptors());
  VeriyViewOrder(0, 1, 2, -1);
  if (HasFatalFailure())
    return;
}

// Tests changing the model while sorted.
TEST_F(TableViewTest, SortThenChange) {
  // Sort by first column descending.
  TableView::SortDescriptors sort;
  sort.push_back(TableView::SortDescriptor(0, false));
  table_->SetSortDescriptors(sort);
  VeriyViewOrder(2, 1, 0, -1);
  if (HasFatalFailure())
    return;

  model_->ChangeRow(0, 3, 1);
  VeriyViewOrder(0, 2, 1, -1);
}

// Tests adding to the model while sorted.
TEST_F(TableViewTest, AddToSorted) {
  // Sort by first column descending.
  TableView::SortDescriptors sort;
  sort.push_back(TableView::SortDescriptor(0, false));
  table_->SetSortDescriptors(sort);
  VeriyViewOrder(2, 1, 0, -1);
  if (HasFatalFailure())
    return;

  // Add row so that it occurs first.
  model_->AddRow(0, 5, -1);
  VeriyViewOrder(0, 3, 2, 1, -1);
  if (HasFatalFailure())
    return;

  // Add row so that it occurs last.
  model_->AddRow(0, -1, -1);
  VeriyViewOrder(1, 4, 3, 2, 0, -1);
}

// Tests selection on sort.
TEST_F(TableViewTest, PersistSelectionOnSort) {
  // Select row 0.
  table_->Select(0);

  // Sort by first column descending.
  TableView::SortDescriptors sort;
  sort.push_back(TableView::SortDescriptor(0, false));
  table_->SetSortDescriptors(sort);
  VeriyViewOrder(2, 1, 0, -1);
  if (HasFatalFailure())
    return;

  // Make sure 0 is still selected.
  EXPECT_EQ(0, table_->FirstSelectedRow());
}

// Tests selection iterator with sort.
TEST_F(TableViewTest, PersistMultiSelectionOnSort) {
  SetUpMultiSelectTestState(true);
}

// Tests selection persists after a change when sorted with iterator.
TEST_F(TableViewTest, PersistMultiSelectionOnChangeWithSort) {
  SetUpMultiSelectTestState(true);
  if (HasFatalFailure())
    return;

  model_->ChangeRow(0, 3, 1);

  VerifySelectedRows(1, 0, -1);
}

// Tests selection persists after a remove when sorted with iterator.
TEST_F(TableViewTest, PersistMultiSelectionOnRemoveWithSort) {
  SetUpMultiSelectTestState(true);
  if (HasFatalFailure())
    return;

  model_->RemoveRow(0);

  VerifySelectedRows(0, -1);
}

// Tests selection persists after a add when sorted with iterator.
TEST_F(TableViewTest, PersistMultiSelectionOnAddWithSort) {
  SetUpMultiSelectTestState(true);
  if (HasFatalFailure())
    return;

  model_->AddRow(3, 4, 4);

  VerifySelectedRows(0, 1, -1);
}

// Tests selection persists after a change with iterator.
TEST_F(TableViewTest, PersistMultiSelectionOnChange) {
  SetUpMultiSelectTestState(false);
  if (HasFatalFailure())
    return;

  model_->ChangeRow(0, 3, 1);

  VerifySelectedRows(1, 0, -1);
}

// Tests selection persists after a remove with iterator.
TEST_F(TableViewTest, PersistMultiSelectionOnRemove) {
  SetUpMultiSelectTestState(false);
  if (HasFatalFailure())
    return;

  model_->RemoveRow(0);

  VerifySelectedRows(0, -1);
}

// Tests selection persists after a add with iterator.
TEST_F(TableViewTest, PersistMultiSelectionOnAdd) {
  SetUpMultiSelectTestState(false);
  if (HasFatalFailure())
    return;

  model_->AddRow(3, 4, 4);

  VerifySelectedRows(1, 0, -1);
}

TEST_F(NullModelTableViewTest, NullModel) {
  // There's nothing explicit to test. If there is a bug in TableView relating
  // to a NULL model we'll crash.
}
