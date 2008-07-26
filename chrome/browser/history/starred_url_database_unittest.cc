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

  StarredEntry GetStarredEntryByURL(const GURL& url) {
    std::vector<StarredEntry> starred;
    EXPECT_TRUE(GetStarredEntries(0, &starred));
    for (std::vector<StarredEntry>::iterator i = starred.begin();
         i != starred.end(); ++i) {
      if (i->url == url)
        return *i;
    }
    EXPECT_TRUE(false);
    return StarredEntry();
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
    GetStarredEntries(0, &entries);
    return static_cast<int>(entries.size());
  }

  bool GetURLStarred(const GURL& url) {
    URLRow row;
    if (GetRowForURL(url, &row))
      return row.starred();
    return false;
  }

  void VerifyInitialState() {
    std::vector<StarredEntry> star_entries;
    EXPECT_TRUE(GetStarredEntries(0, &star_entries));
    EXPECT_EQ(2, star_entries.size());

    int bb_index = -1;
    size_t other_index = -1;
    if (star_entries[0].type == history::StarredEntry::BOOKMARK_BAR) {
      bb_index = 0;
      other_index = 1;
    } else {
      bb_index = 1;
      other_index = 0;
    }
    EXPECT_EQ(HistoryService::kBookmarkBarID, star_entries[bb_index].id);
    EXPECT_EQ(HistoryService::kBookmarkBarID, star_entries[bb_index].group_id);
    EXPECT_EQ(0, star_entries[bb_index].parent_group_id);
    EXPECT_EQ(StarredEntry::BOOKMARK_BAR, star_entries[bb_index].type);

    EXPECT_TRUE(star_entries[other_index].id != HistoryService::kBookmarkBarID
                && star_entries[other_index].id != 0);
    EXPECT_TRUE(star_entries[other_index].group_id !=
                HistoryService::kBookmarkBarID &&
                star_entries[other_index].group_id != 0);
    EXPECT_EQ(0, star_entries[other_index].parent_group_id);
    EXPECT_EQ(StarredEntry::OTHER, star_entries[other_index].type);
  }

 private:
  // Test setup.
  void SetUp() {
    PathService::Get(base::DIR_TEMP, &db_file_);
    db_file_.push_back(file_util::kPathSeparator);
    db_file_.append(L"VisitTest.db");
    file_util::Delete(db_file_, false);

    EXPECT_EQ(SQLITE_OK, sqlite3_open(WideToUTF8(db_file_).c_str(), &db_));
    statement_cache_ = new SqliteStatementCache(db_);

    // Initialize the tables for this test.
    CreateURLTable(false);
    CreateMainURLIndex();
    InitStarTable();
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

TEST_F(StarredURLDatabaseTest, CreateStarredEntryUnstarred) {
  StarredEntry entry;
  entry.title = L"blah";
  entry.date_added = Time::Now();
  entry.url = GURL("http://www.google.com");
  entry.parent_group_id = HistoryService::kBookmarkBarID;

  StarID id = CreateStarredEntry(&entry);

  EXPECT_NE(0, id);
  EXPECT_EQ(id, entry.id);
  CompareEntryByID(entry);
}

TEST_F(StarredURLDatabaseTest, UpdateStarredEntry) {
  const int initial_count = GetStarredEntryCount();
  StarredEntry entry;
  entry.title = L"blah";
  entry.date_added = Time::Now();
  entry.url = GURL("http://www.google.com");
  entry.parent_group_id = HistoryService::kBookmarkBarID;

  StarID id = CreateStarredEntry(&entry);
  EXPECT_NE(0, id);
  EXPECT_EQ(id, entry.id);
  CompareEntryByID(entry);

  // Now invoke create again, as the URL hasn't changed this should just
  // update the title.
  EXPECT_TRUE(UpdateStarredEntry(&entry));
  CompareEntryByID(entry);

  // There should only be two entries (one for the url we created, one for the
  // bookmark bar).
  EXPECT_EQ(initial_count + 1, GetStarredEntryCount());
}

TEST_F(StarredURLDatabaseTest, DeleteStarredGroup) {
  const int initial_count = GetStarredEntryCount();
  StarredEntry entry;
  entry.type = StarredEntry::USER_GROUP;
  entry.title = L"blah";
  entry.date_added = Time::Now();
  entry.group_id = 10;
  entry.parent_group_id = HistoryService::kBookmarkBarID;

  StarID id = CreateStarredEntry(&entry);
  EXPECT_NE(0, id);
  EXPECT_NE(HistoryService::kBookmarkBarID, id);
  CompareEntryByID(entry);

  EXPECT_EQ(initial_count + 1, GetStarredEntryCount());

  // Now delete it.
  std::set<GURL> unstarred_urls;
  std::vector<StarredEntry> deleted_entries;
  DeleteStarredEntry(entry.id, &unstarred_urls, &deleted_entries);
  ASSERT_EQ(0, unstarred_urls.size());
  ASSERT_EQ(1, deleted_entries.size());
  EXPECT_EQ(deleted_entries[0].id, id);

  // Should only have the bookmark bar.
  EXPECT_EQ(initial_count, GetStarredEntryCount());
}

TEST_F(StarredURLDatabaseTest, DeleteStarredGroupWithChildren) {
  const int initial_count = GetStarredEntryCount();

  StarredEntry entry;
  entry.type = StarredEntry::USER_GROUP;
  entry.title = L"blah";
  entry.date_added = Time::Now();
  entry.group_id = 10;
  entry.parent_group_id = HistoryService::kBookmarkBarID;

  StarID id = CreateStarredEntry(&entry);
  EXPECT_NE(0, id);
  EXPECT_NE(HistoryService::kBookmarkBarID, id);
  CompareEntryByID(entry);

  EXPECT_EQ(initial_count + 1, GetStarredEntryCount());

  // Create a child of the group.
  StarredEntry url_entry1;
  url_entry1.parent_group_id = entry.group_id;
  url_entry1.title = L"blah";
  url_entry1.date_added = Time::Now();
  url_entry1.url = GURL("http://www.google.com");
  StarID url_id1 = CreateStarredEntry(&url_entry1);
  EXPECT_NE(0, url_id1);
  CompareEntryByID(url_entry1);

  // Create a sibling of the group.
  StarredEntry url_entry2;
  url_entry2.parent_group_id = HistoryService::kBookmarkBarID;
  url_entry2.title = L"blah";
  url_entry2.date_added = Time::Now();
  url_entry2.url = GURL("http://www.google2.com");
  url_entry2.visual_order = 1;
  StarID url_id2 = CreateStarredEntry(&url_entry2);
  EXPECT_NE(0, url_id2);
  CompareEntryByID(url_entry2);

  EXPECT_EQ(initial_count + 3, GetStarredEntryCount());

  // Now delete the group, which should delete url_entry1 and shift down the
  // visual order of url_entry2.
  std::set<GURL> unstarred_urls;
  std::vector<StarredEntry> deleted_entries;
  DeleteStarredEntry(entry.id, &unstarred_urls, &deleted_entries);
  EXPECT_EQ(2, deleted_entries.size());
  EXPECT_EQ(1, unstarred_urls.size());
  EXPECT_TRUE((deleted_entries[0].id == id) || (deleted_entries[1].id == id));
  EXPECT_TRUE((deleted_entries[0].id == url_id1) ||
              (deleted_entries[1].id == url_id1));

  url_entry2.visual_order--;
  CompareEntryByID(url_entry2);

  // Should have the bookmark bar, and url_entry2.
  EXPECT_EQ(initial_count + 1, GetStarredEntryCount());
}

TEST_F(StarredURLDatabaseTest, InitialState) {
  VerifyInitialState();
}

TEST_F(StarredURLDatabaseTest, CreateStarGroup) {
  const int initial_count = GetStarredEntryCount();
  std::wstring title(L"title");
  Time time(Time::Now());
  int visual_order = 0;

  UIStarID ui_group_id = 10;
  UIStarID ui_parent_group_id = HistoryService::kBookmarkBarID;

  StarredEntry entry;
  entry.type = StarredEntry::USER_GROUP;
  entry.group_id = ui_group_id;
  entry.parent_group_id = ui_parent_group_id;
  entry.title = title;
  entry.date_added = time;
  entry.visual_order = visual_order;
  StarID group_id = CreateStarredEntry(&entry);

  EXPECT_NE(0, group_id);
  EXPECT_NE(HistoryService::kBookmarkBarID, group_id);

  EXPECT_EQ(group_id, GetStarIDForGroupID(ui_group_id));

  StarredEntry group_entry;
  EXPECT_TRUE(GetStarredEntry(group_id, &group_entry));
  EXPECT_EQ(ui_group_id, group_entry.group_id);
  EXPECT_EQ(ui_parent_group_id, group_entry.parent_group_id);
  EXPECT_TRUE(group_entry.title == title);
  // Don't use Time.== here as the conversion to time_t when writing to the db
  // is lossy.
  EXPECT_TRUE(group_entry.date_added.ToTimeT() == time.ToTimeT());
  EXPECT_EQ(visual_order, group_entry.visual_order);

  // Update the date modified.
  Time date_modified = Time::Now();
  entry.date_group_modified = date_modified;
  UpdateStarredEntry(&group_entry);
  EXPECT_TRUE(GetStarredEntry(group_id, &group_entry));
  EXPECT_TRUE(entry.date_group_modified.ToTimeT() == date_modified.ToTimeT());
}

TEST_F(StarredURLDatabaseTest, UpdateStarredEntryChangeVisualOrder) {
  const int initial_count = GetStarredEntryCount();
  GURL url1(L"http://google.com/1");
  GURL url2(L"http://google.com/2");

  // Star url1, making it a child of the bookmark bar.
  StarredEntry entry;
  entry.url = url1;
  entry.title = L"FOO";
  entry.parent_group_id = HistoryService::kBookmarkBarID;
  CreateStarredEntry(&entry);

  // Make sure it was created correctly.
  entry = GetStarredEntryByURL(url1);
  EXPECT_EQ(GetRowForURL(url1, NULL), entry.url_id);
  CompareEntryByID(entry);

  // Star url2, making it a child of the bookmark bar, placing it after url2.
  StarredEntry entry2;
  entry2.url = url2;
  entry2.visual_order = 1;
  entry2.title = std::wstring();
  entry2.parent_group_id = HistoryService::kBookmarkBarID;
  CreateStarredEntry(&entry2);
  entry2 = GetStarredEntryByURL(url2);
  EXPECT_EQ(GetRowForURL(url2, NULL), entry2.url_id);
  CompareEntryByID(entry2);

  // Move url2 to be before url1.
  entry2.visual_order = 0;
  UpdateStarredEntry(&entry2);
  CompareEntryByID(entry2);

  // URL1's visual order should have shifted by one to accommodate URL2.
  entry.visual_order = 1;
  CompareEntryByID(entry);

  // Now move URL1 to position 0, which will move url2 to position 1.
  entry.visual_order = 0;
  UpdateStarredEntry(&entry);
  CompareEntryByID(entry);

  entry2.visual_order = 1;
  CompareEntryByID(entry2);

  // Create a new group between url1 and url2.
  StarredEntry g_entry;
  g_entry.group_id = 10;
  g_entry.parent_group_id = HistoryService::kBookmarkBarID;
  g_entry.title = L"blah";
  g_entry.visual_order = 1;
  g_entry.date_added = Time::Now();
  g_entry.type = StarredEntry::USER_GROUP;
  g_entry.id = CreateStarredEntry(&g_entry);
  EXPECT_NE(0, g_entry.id);
  CompareEntryByID(g_entry);

  // URL2 should have shifted to position 2.
  entry2.visual_order = 2;
  CompareEntryByID(entry2);

  // Move url2 inside of group 1.
  entry2.visual_order = 0;
  entry2.parent_group_id = g_entry.group_id;
  UpdateStarredEntry(&entry2);
  CompareEntryByID(entry2);

  // Move url1 to be a child of group 1 at position 0.
  entry.parent_group_id = g_entry.group_id;
  entry.visual_order = 0;
  UpdateStarredEntry(&entry);
  CompareEntryByID(entry);

  // url2 should have moved to position 1.
  entry2.visual_order = 1;
  CompareEntryByID(entry2);

  // And the group should have shifted to position 0.
  g_entry.visual_order = 0;
  CompareEntryByID(g_entry);

  EXPECT_EQ(initial_count + 3, GetStarredEntryCount());

  // Delete the group, which should unstar url1 and url2.
  std::set<GURL> unstarred_urls;
  std::vector<StarredEntry> deleted_entries;
  DeleteStarredEntry(g_entry.id, &unstarred_urls, &deleted_entries);
  EXPECT_EQ(initial_count, GetStarredEntryCount());
  URLRow url_row;
  GetRowForURL(url1, &url_row);
  EXPECT_EQ(0, url_row.star_id());
  GetRowForURL(url2, &url_row);
  EXPECT_EQ(0, url_row.star_id());
}

TEST_F(StarredURLDatabaseTest, GetMostRecentStarredEntries) {
  // Initially there shouldn't be any entries (only the bookmark bar, which
  // isn't returned by GetMostRecentStarredEntries).
  std::vector<StarredEntry> starred_entries;
  GetMostRecentStarredEntries(10, &starred_entries);
  EXPECT_EQ(0, starred_entries.size());

  // Star url1.
  GURL url1(L"http://google.com/1");
  StarredEntry entry1;
  entry1.type = StarredEntry::URL;
  entry1.url = url1;
  entry1.parent_group_id = HistoryService::kBookmarkBarID;
  CreateStarredEntry(&entry1);

  // Get the recent ones.
  GetMostRecentStarredEntries(10, &starred_entries);
  ASSERT_EQ(1, starred_entries.size());
  EXPECT_TRUE(starred_entries[0].url == url1);

  // Add url2 and star it.
  GURL url2(L"http://google.com/2");
  StarredEntry entry2;
  entry2.type = StarredEntry::URL;
  entry2.url = url2;
  entry2.parent_group_id = HistoryService::kBookmarkBarID;
  CreateStarredEntry(&entry2);

  starred_entries.clear();
  GetMostRecentStarredEntries(10, &starred_entries);
  ASSERT_EQ(2, starred_entries.size());
  EXPECT_TRUE(starred_entries[0].url == url2);
  EXPECT_TRUE(starred_entries[1].url == url1);
}

TEST_F(StarredURLDatabaseTest, FixOrphanedGroup) {
  check_starred_integrity_on_mutation_ = false;

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
  check_starred_integrity_on_mutation_ = false;

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
  check_starred_integrity_on_mutation_ = false;

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
  check_starred_integrity_on_mutation_ = false;

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
  check_starred_integrity_on_mutation_ = false;

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

TEST_F(StarredURLDatabaseTest, RestoreOtherAndBookmarkBar) {
  std::vector<StarredEntry> entries;
  GetStarredEntries(0, &entries);

  check_starred_integrity_on_mutation_ = false;

  for (std::vector<StarredEntry>::iterator i = entries.begin();
       i != entries.end(); ++i) {
    if (i->type != StarredEntry::USER_GROUP) {
      // Use this directly, otherwise we assert trying to delete other/bookmark
      // bar.
      DeleteStarredEntryRow(i->id);

      ASSERT_TRUE(EnsureStarredIntegrity());

      VerifyInitialState();
    }
  }
}

TEST_F(StarredURLDatabaseTest, FixDuplicateGroupIDs) {
  check_starred_integrity_on_mutation_ = false;

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
