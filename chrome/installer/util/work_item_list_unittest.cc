// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>

#include "base/base_paths.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "base/registry.h"
#include "base/scoped_ptr.h"
#include "base/string_util.h"
#include "chrome/installer/util/work_item.h"
#include "chrome/installer/util/work_item_list.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
  wchar_t test_root[] = L"ListList";
  wchar_t data_str[] = L"data_111";

  class WorkItemListTest : public testing::Test {
   protected:
    virtual void SetUp() {
      // Create a temporary key for testing
      RegKey key(HKEY_CURRENT_USER, L"", KEY_ALL_ACCESS);
      key.DeleteKey(test_root);
      ASSERT_FALSE(key.Open(HKEY_CURRENT_USER, test_root, KEY_READ));
      ASSERT_TRUE(key.Create(HKEY_CURRENT_USER, test_root, KEY_READ));

      // Create a temp directory for test.
      ASSERT_TRUE(PathService::Get(base::DIR_TEMP, &test_dir_));
      file_util::AppendToPath(&test_dir_, L"WorkItemListTest");
      file_util::Delete(test_dir_, true);
      ASSERT_FALSE(file_util::PathExists(test_dir_));
      CreateDirectory(test_dir_.c_str(), NULL);
      ASSERT_TRUE(file_util::PathExists(test_dir_));
    }

    virtual void TearDown() {
      logging::CloseLogFile();
      // Clean up test directory
      ASSERT_TRUE(file_util::Delete(test_dir_, true));
      ASSERT_FALSE(file_util::PathExists(test_dir_));
      // Clean up the temporary key
      RegKey key(HKEY_CURRENT_USER, L"", KEY_ALL_ACCESS);
      ASSERT_TRUE(key.DeleteKey(test_root));
    }

    std::wstring test_dir_;
  };
};

// Execute a WorkItem list successfully and then rollback.
TEST_F(WorkItemListTest, ExecutionSuccess) {
  scoped_ptr<WorkItemList> work_item_list(WorkItem::CreateWorkItemList());
  scoped_ptr<WorkItem> work_item;

  std::wstring top_dir_to_create(test_dir_);
  file_util::AppendToPath(&top_dir_to_create, L"a");
  std::wstring dir_to_create(top_dir_to_create);
  file_util::AppendToPath(&dir_to_create, L"b");
  ASSERT_FALSE(file_util::PathExists(dir_to_create));

  work_item.reset(reinterpret_cast<WorkItem*>(
      WorkItem::CreateCreateDirWorkItem(dir_to_create)));
  EXPECT_TRUE(work_item_list->AddWorkItem(work_item.release()));

  std::wstring key_to_create(test_root);
  file_util::AppendToPath(&key_to_create, L"ExecutionSuccess");

  work_item.reset(reinterpret_cast<WorkItem*>(
      WorkItem::CreateCreateRegKeyWorkItem(HKEY_CURRENT_USER, key_to_create)));
  EXPECT_TRUE(work_item_list->AddWorkItem(work_item.release()));

  std::wstring name(L"name");
  std::wstring data(data_str);
  work_item.reset(reinterpret_cast<WorkItem*>(
      WorkItem::CreateSetRegValueWorkItem(HKEY_CURRENT_USER, key_to_create,
                                          name, data, false)));
  EXPECT_TRUE(work_item_list->AddWorkItem(work_item.release()));

  EXPECT_TRUE(work_item_list->Do());

  // Verify all WorkItems have been executed.
  RegKey key;
  EXPECT_TRUE(key.Open(HKEY_CURRENT_USER, key_to_create.c_str(), KEY_READ));
  std::wstring read_out;
  EXPECT_TRUE(key.ReadValue(name.c_str(), &read_out));
  EXPECT_EQ(0, read_out.compare(data_str));
  key.Close();
  EXPECT_TRUE(file_util::PathExists(dir_to_create));

  work_item_list->Rollback();

  // Verify everything is rolled back.
  // The value must have been deleted first in roll back otherwise the key
  // can not be deleted.
  EXPECT_FALSE(key.Open(HKEY_CURRENT_USER, key_to_create.c_str(), KEY_READ));
  EXPECT_FALSE(file_util::PathExists(top_dir_to_create));
}

// Execute a WorkItem list. Fail in the middle. Rollback what has been done.
TEST_F(WorkItemListTest, ExecutionFailAndRollback) {
  scoped_ptr<WorkItemList> work_item_list(WorkItem::CreateWorkItemList());
  scoped_ptr<WorkItem> work_item;

  std::wstring top_dir_to_create(test_dir_);
  file_util::AppendToPath(&top_dir_to_create, L"a");
  std::wstring dir_to_create(top_dir_to_create);
  file_util::AppendToPath(&dir_to_create, L"b");
  ASSERT_FALSE(file_util::PathExists(dir_to_create));

  work_item.reset(reinterpret_cast<WorkItem*>(
      WorkItem::CreateCreateDirWorkItem(dir_to_create)));
  EXPECT_TRUE(work_item_list->AddWorkItem(work_item.release()));

  std::wstring key_to_create(test_root);
  file_util::AppendToPath(&key_to_create, L"ExecutionFail");

  work_item.reset(reinterpret_cast<WorkItem*>(
      WorkItem::CreateCreateRegKeyWorkItem(HKEY_CURRENT_USER, key_to_create)));
  EXPECT_TRUE(work_item_list->AddWorkItem(work_item.release()));

  std::wstring not_created_key(test_root);
  file_util::AppendToPath(&not_created_key, L"NotCreated");
  std::wstring name(L"name");
  std::wstring data(data_str);
  work_item.reset(reinterpret_cast<WorkItem*>(
      WorkItem::CreateSetRegValueWorkItem(HKEY_CURRENT_USER, not_created_key,
                                          name, data, false)));
  EXPECT_TRUE(work_item_list->AddWorkItem(work_item.release()));

  // This one will not be executed because we will fail early.
  work_item.reset(reinterpret_cast<WorkItem*>(
      WorkItem::CreateCreateRegKeyWorkItem(HKEY_CURRENT_USER,
                                           not_created_key)));
  EXPECT_TRUE(work_item_list->AddWorkItem(work_item.release()));

  EXPECT_FALSE(work_item_list->Do());

  // Verify the first 2 WorkItems have been executed.
  RegKey key;
  EXPECT_TRUE(key.Open(HKEY_CURRENT_USER, key_to_create.c_str(), KEY_READ));
  key.Close();
  EXPECT_TRUE(file_util::PathExists(dir_to_create));
  // The last one should not be there.
  EXPECT_FALSE(key.Open(HKEY_CURRENT_USER, not_created_key.c_str(),
               KEY_READ));

  work_item_list->Rollback();

  // Verify everything is rolled back.
  EXPECT_FALSE(key.Open(HKEY_CURRENT_USER, key_to_create.c_str(), KEY_READ));
  EXPECT_FALSE(file_util::PathExists(top_dir_to_create));
}
