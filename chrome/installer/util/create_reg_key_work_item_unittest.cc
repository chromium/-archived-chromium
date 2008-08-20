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

#include <windows.h>

#include "base/file_util.h"
#include "base/logging.h"
#include "base/registry.h"
#include "base/scoped_ptr.h"
#include "base/string_util.h"
#include "chrome/installer/util/create_reg_key_work_item.h"
#include "chrome/installer/util/work_item.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
  wchar_t test_root[] = L"TmpTmp";
  class CreateRegKeyWorkItemTest : public testing::Test {
   protected:
    virtual void SetUp() {
      // Create a temporary key for testing
      RegKey key(HKEY_CURRENT_USER, L"", KEY_ALL_ACCESS);
      key.DeleteKey(test_root);
      ASSERT_FALSE(key.Open(HKEY_CURRENT_USER, test_root, KEY_READ));
      ASSERT_TRUE(key.Create(HKEY_CURRENT_USER, test_root, KEY_READ));
    }
    virtual void TearDown() {
      logging::CloseLogFile();
      // Clean up the temporary key
      RegKey key(HKEY_CURRENT_USER, L"", KEY_ALL_ACCESS);
      ASSERT_TRUE(key.DeleteKey(test_root));
    }
  };
};

TEST_F(CreateRegKeyWorkItemTest, CreateKey) {
  RegKey key;

  std::wstring parent_key(test_root);
  file_util::AppendToPath(&parent_key, L"a");
  ASSERT_TRUE(key.Create(HKEY_CURRENT_USER, parent_key.c_str(), KEY_READ));

  std::wstring top_key_to_create(parent_key);
  file_util::AppendToPath(&top_key_to_create, L"b");

  std::wstring key_to_create(top_key_to_create);
  file_util::AppendToPath(&key_to_create, L"c");
  file_util::AppendToPath(&key_to_create, L"d");

  scoped_ptr<CreateRegKeyWorkItem> work_item(
      WorkItem::CreateCreateRegKeyWorkItem(HKEY_CURRENT_USER, key_to_create));

  EXPECT_TRUE(work_item->Do());

  EXPECT_TRUE(key.Open(HKEY_CURRENT_USER, key_to_create.c_str(), KEY_READ));

  work_item->Rollback();

  // Rollback should delete all the keys up to top_key_to_create.
  EXPECT_FALSE(key.Open(HKEY_CURRENT_USER, top_key_to_create.c_str(),
                        KEY_READ));
  EXPECT_TRUE(key.Open(HKEY_CURRENT_USER, parent_key.c_str(), KEY_READ));
}

TEST_F(CreateRegKeyWorkItemTest, CreateExistingKey) {
  RegKey key;

  std::wstring key_to_create(test_root);
  file_util::AppendToPath(&key_to_create, L"aa");
  ASSERT_TRUE(key.Create(HKEY_CURRENT_USER, key_to_create.c_str(), KEY_READ));

  scoped_ptr<CreateRegKeyWorkItem> work_item(
      WorkItem::CreateCreateRegKeyWorkItem(HKEY_CURRENT_USER, key_to_create));

  EXPECT_TRUE(work_item->Do());

  EXPECT_TRUE(key.Open(HKEY_CURRENT_USER, key_to_create.c_str(), KEY_READ));

  work_item->Rollback();

  // Rollback should not remove the key since it exists before
  // the CreateRegKeyWorkItem is called.
  EXPECT_TRUE(key.Open(HKEY_CURRENT_USER, key_to_create.c_str(), KEY_READ));
}

TEST_F(CreateRegKeyWorkItemTest, CreateSharedKey) {
  RegKey key;
  std::wstring key_to_create_1(test_root);
  file_util::AppendToPath(&key_to_create_1, L"aaa");

  std::wstring key_to_create_2(key_to_create_1);
  file_util::AppendToPath(&key_to_create_2, L"bbb");

  std::wstring key_to_create_3(key_to_create_2);
  file_util::AppendToPath(&key_to_create_3, L"ccc");

  scoped_ptr<CreateRegKeyWorkItem> work_item(
      WorkItem::CreateCreateRegKeyWorkItem(HKEY_CURRENT_USER,
                                           key_to_create_3));

  EXPECT_TRUE(work_item->Do());

  EXPECT_TRUE(key.Open(HKEY_CURRENT_USER, key_to_create_3.c_str(), KEY_READ));

  // Create another key under key_to_create_2
  std::wstring key_to_create_4(key_to_create_2);
  file_util::AppendToPath(&key_to_create_4, L"ddd");
  ASSERT_TRUE(key.Create(HKEY_CURRENT_USER, key_to_create_4.c_str(),
                         KEY_READ));

  work_item->Rollback();

  // Rollback should delete key_to_create_3.
  EXPECT_FALSE(key.Open(HKEY_CURRENT_USER, key_to_create_3.c_str(), KEY_READ));

  // Rollback should not delete key_to_create_2 as it is shared.
  EXPECT_TRUE(key.Open(HKEY_CURRENT_USER, key_to_create_2.c_str(), KEY_READ));
  EXPECT_TRUE(key.Open(HKEY_CURRENT_USER, key_to_create_4.c_str(), KEY_READ));
}

TEST_F(CreateRegKeyWorkItemTest, RollbackWithMissingKey) {
  RegKey key;
  std::wstring key_to_create_1(test_root);
  file_util::AppendToPath(&key_to_create_1, L"aaaa");

  std::wstring key_to_create_2(key_to_create_1);
  file_util::AppendToPath(&key_to_create_2, L"bbbb");

  std::wstring key_to_create_3(key_to_create_2);
  file_util::AppendToPath(&key_to_create_3, L"cccc");

  scoped_ptr<CreateRegKeyWorkItem> work_item(
      WorkItem::CreateCreateRegKeyWorkItem(HKEY_CURRENT_USER,
                                           key_to_create_3));

  EXPECT_TRUE(work_item->Do());

  EXPECT_TRUE(key.Open(HKEY_CURRENT_USER, key_to_create_3.c_str(), KEY_READ));
  key.Close();

  // now delete key_to_create_3
  ASSERT_TRUE(RegDeleteKey(HKEY_CURRENT_USER, key_to_create_3.c_str()) ==
              ERROR_SUCCESS);
  ASSERT_FALSE(key.Open(HKEY_CURRENT_USER, key_to_create_3.c_str(),
                        KEY_READ));

  work_item->Rollback();

  // key_to_create_3 has already been deleted, Rollback should delete
  // the rest.
  ASSERT_FALSE(key.Open(HKEY_CURRENT_USER, key_to_create_1.c_str(),
                        KEY_READ));
}

TEST_F(CreateRegKeyWorkItemTest, RollbackWithSetValue) {
  RegKey key;

  std::wstring key_to_create(test_root);
  file_util::AppendToPath(&key_to_create, L"aaaaa");

  scoped_ptr<CreateRegKeyWorkItem> work_item(
      WorkItem::CreateCreateRegKeyWorkItem(HKEY_CURRENT_USER, key_to_create));

  EXPECT_TRUE(work_item->Do());

  // Write a value under the key we just created.
  EXPECT_TRUE(key.Open(HKEY_CURRENT_USER, key_to_create.c_str(),
                       KEY_READ | KEY_SET_VALUE));
  EXPECT_TRUE(key.WriteValue(L"name", L"value"));
  key.Close();

  work_item->Rollback();

  // Rollback should not remove the key.
  EXPECT_TRUE(key.Open(HKEY_CURRENT_USER, key_to_create.c_str(), KEY_READ));
}
