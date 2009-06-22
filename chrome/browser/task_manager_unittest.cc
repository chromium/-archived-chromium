// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/task_manager.h"

#include <string>

#include "app/l10n_util.h"
#include "base/process_util.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class TestResource : public TaskManager::Resource {
 public:
  virtual std::wstring GetTitle() const { return L"test title"; }
  virtual SkBitmap GetIcon() const { return SkBitmap(); }
  virtual base::ProcessHandle GetProcess() const {
    return base::GetCurrentProcessHandle();
  }
  virtual bool SupportNetworkUsage() const { return false; }
  virtual void SetSupportNetworkUsage() { NOTREACHED(); }
};

}  // namespace

class TaskManagerTest : public testing::Test {
};

TEST_F(TaskManagerTest, Basic) {
  TaskManager task_manager;
  TaskManagerModel* model = task_manager.model_;
  EXPECT_EQ(0, model->ResourceCount());
}

TEST_F(TaskManagerTest, Resources) {
  TaskManager task_manager;
  TaskManagerModel* model = task_manager.model_;

  TestResource resource1, resource2;

  task_manager.AddResource(&resource1);
  ASSERT_EQ(1, model->ResourceCount());
  EXPECT_TRUE(model->IsResourceFirstInGroup(0));
  EXPECT_STREQ(L"test title", model->GetResourceTitle(0).c_str());
  EXPECT_STREQ(l10n_util::GetString(IDS_TASK_MANAGER_NA_CELL_TEXT).c_str(),
               model->GetResourceNetworkUsage(0).c_str());
  EXPECT_STREQ(L"0", model->GetResourceCPUUsage(0).c_str());

  task_manager.AddResource(&resource2);  // Will be in the same group.
  ASSERT_EQ(2, model->ResourceCount());
  EXPECT_TRUE(model->IsResourceFirstInGroup(0));
  EXPECT_FALSE(model->IsResourceFirstInGroup(1));
  EXPECT_STREQ(L"test title", model->GetResourceTitle(1).c_str());
  EXPECT_STREQ(l10n_util::GetString(IDS_TASK_MANAGER_NA_CELL_TEXT).c_str(),
               model->GetResourceNetworkUsage(1).c_str());
  EXPECT_STREQ(L"0", model->GetResourceCPUUsage(1).c_str());

  task_manager.RemoveResource(&resource1);
  // Now resource2 will be first in group.
  ASSERT_EQ(1, model->ResourceCount());
  EXPECT_TRUE(model->IsResourceFirstInGroup(0));
  EXPECT_STREQ(L"test title", model->GetResourceTitle(0).c_str());
  EXPECT_STREQ(l10n_util::GetString(IDS_TASK_MANAGER_NA_CELL_TEXT).c_str(),
               model->GetResourceNetworkUsage(0).c_str());
  EXPECT_STREQ(L"0", model->GetResourceCPUUsage(0).c_str());

  task_manager.RemoveResource(&resource2);
  EXPECT_EQ(0, model->ResourceCount());
}
