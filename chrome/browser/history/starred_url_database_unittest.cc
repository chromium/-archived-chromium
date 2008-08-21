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

#include <vector>

#include "base/file_util.h"
#include "base/path_service.h"
#include "base/string_util.h"
#include "chrome/browser/history/history.h"
#include "chrome/browser/history/starred_url_database.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/sqlite_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace history {

class StarredURLDatabaseTest : public testing::Test,
                               public StarredURLDatabase {
 public:
  StarredURLDatabaseTest() : db_(NULL), statement_cache_(NULL) {
  }

  void AddPage(const GURL& url) {
    URLRow row(url);
    row.set_visit_count(1);
    EXPECT_TRUE(AddURL(row));
  }

  void CompareEntryByID(const StarredEntry& entry) {
    DCHECK(entry.id != 0);
    StarredEntry db_value;
    EXPECT_TRUE(GetStarredEntry(entry.id, &db_value));
    EXPECT_EQ(entry.id, db_value.id);
    EXPECT_TRUE(entry.title == db_value.title);
    EXPECT_EQ(entry.date_added.ToTimeT(), db_value.date_added.ToTimeT());
    EXPECT_EQ(entry.group_id, db_value.group_id);
    EXPECT_EQ(entry.parent_group_id, db_value.parent_group_id);
    EXPECT_EQ(entry.visual_order, db_value.visual_order);
    EXPECT_EQ(entry.type, db_value.type);
    EXPECT_EQ(entry.url_id, db_value.url_id);
    if (entry.type == StarredEntry::URL)
      EXPECT_TRUE(entry.url == db_value.url);
  }

  int GetStarredEntryCount() {
    DCHECK(db_);
    std::vector<StarredEntry> entries;
    GetAllStarredEntries(&entries);
    return static_cast<int>(entries.size());
  }

  StarID CreateStarredEntry(StarredEntry* entry) {
    return StarredURLDatabase::CreateStarredEntry(entry);
  }

  bool GetStarredEntry(StarID star_id, StarredEntry* entry) {
    return StarredURLDatabase::GetStarredEntry(star_id, entry);
  }

  bool EnsureStarredIntegrity() {
    return StarredURLDatabase::EnsureStarredIntegrity();
  }

 private:
  // Test setup.
  void SetUp() {
    PathService::Get(base::DIR_TEMP, &db_file_);
    db_file_.push_back(file_util::kPathSeparator);
    db_file_.append(L"VisitTest.db");
    file_util::Delete(db_file_, false);

    // Copy db file over that contains starred table.
    std::wstring old_history_path;
    PathService::Get(chrome::DIR_TEST_DATA, &old_history_path);
    file_util::AppendToPath(&old_history_path, L"bookmarks");
    file_util::AppendToPath(&old_history_path, L"History_with_empty_starred");
    file_util::CopyFile(old_history_path, db_file_);

    EXPECT_EQ(SQLITE_OK, sqlite3_open(WideToUTF8(db_file_).c_str(), &db_));
    statement_cache_ = new SqliteStatementCache(db_);

    // Initialize the tables for this test.
    CreateURLTable(false);
    CreateMainURLIndex();
    EnsureStarredIntegrity();
  }
  void TearDown() {
    delete statement_cache_;
    sqlite3_close(db_);
    file_util::Delete(db_file_, false);
  }

  // Provided for URL/StarredURLDatabase.
  virtual sqlite3* GetDB() {
    return db_;
  }
  virtual SqliteStatementCache& GetStatementCache() {
    return *statement_cache_;
  }

  std::wstring db_file_;
  sqlite3* db_;
  SqliteStatementCache* statement_cache_;
};

//-----------------------------------------------------------------------------

TEST_F(StarredURLDatabaseTest, FixOrphanedGroup) {
  const size_t initial_count = GetStarredEntryCount();

  // Create a group that isn't parented to the other/bookmark folders.
  StarredEntry g_entry;
  g_entry.type = StarredEntry::USER_GROUP;
  g_entry.parent_group_id = 100;
  g_entry.visual_order = 10;
  g_entry.group_id = 100;
  CreateStarredEntry(&g_entry);

  ASSERT_TRUE(EnsureStarredIntegrity());

  // Make sure no new entries were added.
  ASSERT_EQ(initial_count + 1, GetStarredEntryCount());

  // Make sure the group was moved to the bookmark bar folder.
  ASSERT_TRUE(GetStarredEntry(g_entry.id, &g_entry));
  ASSERT_EQ(HistoryService::kBookmarkBarID, g_entry.parent_group_id);
  ASSERT_EQ(0, g_entry.visual_order);
}

TEST_F(StarredURLDatabaseTest, FixOrphanedBookmarks) {
  const size_t initial_count = GetStarredEntryCount();

  // Create two bookmarks that aren't in a random folder no on the bookmark bar.
  StarredEntry entry1;
  entry1.parent_group_id = 100;
  entry1.visual_order = 10;
  entry1.url = GURL(L"http://google.com/1");
  CreateStarredEntry(&entry1);

  StarredEntry entry2;
  entry2.parent_group_id = 101;
  entry2.visual_order = 20;
  entry2.url = GURL(L"http://google.com/2");
  CreateStarredEntry(&entry2);

  ASSERT_TRUE(EnsureStarredIntegrity());

  // Make sure no new entries were added.
  ASSERT_EQ(initial_count + 2, GetStarredEntryCount());

  // Make sure the entries were moved to the bookmark bar and the visual order
  // order was updated appropriately.
  ASSERT_TRUE(GetStarredEntry(entry1.id, &entry1));
  ASSERT_EQ(HistoryService::kBookmarkBarID, entry1.parent_group_id);

  ASSERT_TRUE(GetStarredEntry(entry2.id, &entry2));
  ASSERT_EQ(HistoryService::kBookmarkBarID, entry2.parent_group_id);
  ASSERT_TRUE((entry1.visual_order == 0 && entry2.visual_order == 1) ||
              (entry1.visual_order == 1 && entry2.visual_order == 0));
}

TEST_F(StarredURLDatabaseTest, FixGroupCycleDepth0) {
  const size_t initial_count = GetStarredEntryCount();

  // Create a group that is parented to itself.
  StarredEntry entry1;
  entry1.group_id = entry1.parent_group_id = 100;
  entry1.visual_order = 10;
  entry1.type = StarredEntry::USER_GROUP;
  CreateStarredEntry(&entry1);

  ASSERT_TRUE(EnsureStarredIntegrity());

  // Make sure no new entries were added.
  ASSERT_EQ(initial_count + 1, GetStarredEntryCount());

  // Make sure the group were moved to the bookmark bar and the visual order
  // order was updated appropriately.
  ASSERT_TRUE(GetStarredEntry(entry1.id, &entry1));
  ASSERT_EQ(HistoryService::kBookmarkBarID, entry1.parent_group_id);
  ASSERT_EQ(0, entry1.visual_order);
}

TEST_F(StarredURLDatabaseTest, FixGroupCycleDepth1) {
  const size_t initial_count = GetStarredEntryCount();

  StarredEntry entry1;
  entry1.group_id = 100;
  entry1.parent_group_id = 101;
  entry1.visual_order = 10;
  entry1.type = StarredEntry::USER_GROUP;
  CreateStarredEntry(&entry1);

  StarredEntry entry2;
  entry2.group_id = 101;
  entry2.parent_group_id = 100;
  entry2.visual_order = 11;
  entry2.type = StarredEntry::USER_GROUP;
  CreateStarredEntry(&entry2);

  ASSERT_TRUE(EnsureStarredIntegrity());

  // Make sure no new entries were added.
  ASSERT_EQ(initial_count + 2, GetStarredEntryCount());

  // Because the groups caused a cycle, entry1 is moved the bookmark bar, which
  // breaks the cycle.
  ASSERT_TRUE(GetStarredEntry(entry1.id, &entry1));
  ASSERT_TRUE(GetStarredEntry(entry2.id, &entry2));
  ASSERT_EQ(HistoryService::kBookmarkBarID, entry1.parent_group_id);
  ASSERT_EQ(100, entry2.parent_group_id);
  ASSERT_EQ(0, entry1.visual_order);
  ASSERT_EQ(0, entry2.visual_order);
}

TEST_F(StarredURLDatabaseTest, FixVisualOrder) {
  const size_t initial_count = GetStarredEntryCount();

  // Star two urls.
  StarredEntry entry1;
  entry1.url = GURL(L"http://google.com/1");
  entry1.parent_group_id = HistoryService::kBookmarkBarID;
  entry1.visual_order = 5;
  CreateStarredEntry(&entry1);

  // Add url2 and star it.
  StarredEntry entry2;
  entry2.url = GURL(L"http://google.com/2");
  entry2.parent_group_id = HistoryService::kBookmarkBarID;
  entry2.visual_order = 10;
  CreateStarredEntry(&entry2);

  ASSERT_TRUE(EnsureStarredIntegrity());

  // Make sure no new entries were added.
  ASSERT_EQ(initial_count + 2, GetStarredEntryCount());

  StarredEntry entry;
  ASSERT_TRUE(GetStarredEntry(entry1.id, &entry));
  entry1.visual_order = 0;
  CompareEntryByID(entry1);

  ASSERT_TRUE(GetStarredEntry(entry2.id, &entry));
  entry2.visual_order = 1;
  CompareEntryByID(entry2);
}

TEST_F(StarredURLDatabaseTest, FixDuplicateGroupIDs) {
  const size_t initial_count = GetStarredEntryCount();

  // Create two groups with the same group id.
  StarredEntry entry1;
  entry1.type = StarredEntry::USER_GROUP;
  entry1.group_id = 10;
  entry1.parent_group_id = HistoryService::kBookmarkBarID;
  CreateStarredEntry(&entry1);
  StarredEntry entry2 = entry1;
  CreateStarredEntry(&entry2);

  ASSERT_TRUE(EnsureStarredIntegrity());

  // Make sure only one group exists.
  ASSERT_EQ(initial_count + 1, GetStarredEntryCount());

  StarredEntry entry;
  ASSERT_TRUE(GetStarredEntry(entry1.id, &entry) ||
              GetStarredEntry(entry2.id, &entry));
}

TEST_F(StarredURLDatabaseTest, RemoveStarredEntriesWithEmptyURL) {
  const int initial_count = GetStarredEntryCount();

  StarredEntry entry;
  entry.url = GURL("http://google.com");
  entry.title = L"FOO";
  entry.parent_group_id = HistoryService::kBookmarkBarID;

  ASSERT_NE(0, CreateStarredEntry(&entry));

  // Remove the URL.
  DeleteURLRow(entry.url_id);

  // Fix up the table.
  ASSERT_TRUE(EnsureStarredIntegrity());

  // The entry we just created should have been nuked.
  ASSERT_EQ(initial_count, GetStarredEntryCount());
}

}  // namespace history
