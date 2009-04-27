// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/task_manager.h"

#include <string>

#include "chrome/views/controls/table/table_view.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class TestResource : public TaskManager::Resource {
 public:
  virtual std::wstring GetTitle() const { return L"test title"; }
  virtual SkBitmap GetIcon() const { return SkBitmap(); }
  virtual HANDLE GetProcess() const { return NULL; }
  virtual bool SupportNetworkUsage() const { return false; }
  virtual void SetSupportNetworkUsage() { NOTREACHED(); }
};

}  // namespace

class TaskManagerTest : public testing::Test,
                        public views::TableModelObserver {
 public:
  // TableModelObserver
  virtual void OnModelChanged() {}
  virtual void OnItemsChanged(int start, int length) {}
  virtual void OnItemsAdded(int start, int length) {}
  virtual void OnItemsRemoved(int start, int length) {}
};

TEST_F(TaskManagerTest, Basic) {
  TaskManager task_manager;
  TaskManagerTableModel* model = task_manager.table_model_;
  EXPECT_FALSE(task_manager.BrowserProcessIsSelected());
  EXPECT_EQ(0, model->RowCount());
}

TEST_F(TaskManagerTest, Resources) {
  TaskManager task_manager;
  TaskManagerTableModel* model = task_manager.table_model_;
  model->SetObserver(this);

  TestResource resource1, resource2;

  task_manager.AddResource(&resource1);
  ASSERT_EQ(1, model->RowCount());
  EXPECT_STREQ(L"test title",
      model->GetText(0, IDS_TASK_MANAGER_PAGE_COLUMN).c_str());
  EXPECT_STREQ(l10n_util::GetString(IDS_TASK_MANAGER_NA_CELL_TEXT).c_str(),
      model->GetText(0, IDS_TASK_MANAGER_NET_COLUMN).c_str());
  EXPECT_STREQ(L"0",
      model->GetText(0, IDS_TASK_MANAGER_CPU_COLUMN).c_str());

  task_manager.AddResource(&resource2);  // Will be in the same group.
  ASSERT_EQ(2, model->RowCount());
  EXPECT_STREQ(L"test title",
      model->GetText(1, IDS_TASK_MANAGER_PAGE_COLUMN).c_str());
  EXPECT_STREQ(l10n_util::GetString(IDS_TASK_MANAGER_NA_CELL_TEXT).c_str(),
      model->GetText(1, IDS_TASK_MANAGER_NET_COLUMN).c_str());
  EXPECT_STREQ(L"", model->GetText(1, IDS_TASK_MANAGER_CPU_COLUMN).c_str());

  task_manager.RemoveResource(&resource1);
  // Now resource2 will be first in group.
  ASSERT_EQ(1, model->RowCount());
  EXPECT_STREQ(L"test title",
      model->GetText(0, IDS_TASK_MANAGER_PAGE_COLUMN).c_str());
  EXPECT_STREQ(l10n_util::GetString(IDS_TASK_MANAGER_NA_CELL_TEXT).c_str(),
      model->GetText(0, IDS_TASK_MANAGER_NET_COLUMN).c_str());
  EXPECT_STREQ(L"0",
      model->GetText(0, IDS_TASK_MANAGER_CPU_COLUMN).c_str());

  task_manager.RemoveResource(&resource2);
  EXPECT_EQ(0, model->RowCount());
}
