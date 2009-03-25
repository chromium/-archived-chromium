// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"

#include "base/file_util.h"
#include "base/path_service.h"
#include "base/time.h"
#include "chrome/browser/bookmarks/bookmark_html_writer.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/importer/firefox2_importer.h"

class BookmarkHTMLWriterTest : public testing::Test {
 protected:
  virtual void SetUp() {
    ASSERT_TRUE(PathService::Get(base::DIR_TEMP, &path_));
    file_util::AppendToPath(&path_, L"bookmarks.html");
    file_util::Delete(path_, true);

  }

  virtual void TearDown() {
    if (!path_.empty())
      file_util::Delete(path_, true);
  }

  void AssertBookmarkEntryEquals(const ProfileWriter::BookmarkEntry& entry,
                                 bool on_toolbar,
                                 const GURL& url,
                                 const std::wstring& title,
                                 base::Time creation_time,
                                 const std::wstring& f1,
                                 const std::wstring& f2,
                                 const std::wstring& f3) {
    EXPECT_EQ(on_toolbar, entry.in_toolbar);
    EXPECT_TRUE(url == entry.url);
    EXPECT_EQ(title, entry.title);
    EXPECT_TRUE(creation_time.ToTimeT() == entry.creation_time.ToTimeT());
    size_t path_count = 0;
    if (!f3.empty())
      path_count = 3;
    else if (!f2.empty())
      path_count = 2;
    else if (!f1.empty())
      path_count = 1;
    // The first path element should always be 'x', as that is what we passed
    // to the importer.
    ASSERT_EQ(path_count + 1, entry.path.size());
    EXPECT_EQ(L"x", entry.path[0]);
    EXPECT_TRUE(path_count < 1 || entry.path[1] == f1);
    EXPECT_TRUE(path_count < 2 || entry.path[2] == f2);
    EXPECT_TRUE(path_count < 3 || entry.path[3] == f3);
  }

  std::wstring path_;
};

// Tests bookmark_html_writer by populating a BookmarkModel, writing it out by
// way of bookmark_html_writer, then using the importer to read it back in.
TEST_F(BookmarkHTMLWriterTest, Test) {
  // Populate the BookmarkModel. This creates the following bookmark structure:
  // Bookmarks bar
  //   F1
  //     url1
  //     F2
  //       url2
  //   url3
  // Other
  //   url1
  //   url2
  //   F3
  //     F4
  //       url1
  std::wstring f1_title = L"F\"&;<1\"";
  std::wstring f2_title = L"F2";
  std::wstring f3_title = L"F 3";
  std::wstring f4_title = L"F4";
  std::wstring url1_title = L"url 1";
  std::wstring url2_title = L"url&2";
  std::wstring url3_title = L"url\"3";
  GURL url1("http://url1");
  GURL url2("http://url2");
  GURL url3("http://url3");
  BookmarkModel model(NULL);
  base::Time t1(base::Time::Now());
  base::Time t2(t1 + base::TimeDelta::FromHours(1));
  base::Time t3(t1 + base::TimeDelta::FromHours(1));
  BookmarkNode* f1 = model.AddGroup(model.GetBookmarkBarNode(), 0, f1_title);
  model.AddURLWithCreationTime(f1, 0, url1_title, url1, t1);
  BookmarkNode* f2 = model.AddGroup(f1, 1, f2_title);
  model.AddURLWithCreationTime(f2, 0, url2_title, url2, t2);
  model.AddURLWithCreationTime(model.GetBookmarkBarNode(), 1, url3_title, url3,
                               t3);

  model.AddURLWithCreationTime(model.other_node(), 0, url1_title, url1, t1);
  model.AddURLWithCreationTime(model.other_node(), 1, url2_title, url2, t2);
  BookmarkNode* f3 = model.AddGroup(model.other_node(), 2, f3_title);
  BookmarkNode* f4 = model.AddGroup(f3, 0, f4_title);
  model.AddURLWithCreationTime(f4, 0, url1_title, url1, t1);

  // Write to a temp file.
  bookmark_html_writer::WriteBookmarks(NULL, &model, path_);

  // Read the bookmarks back in.
  std::vector<ProfileWriter::BookmarkEntry> parsed_bookmarks;
  Firefox2Importer::ImportBookmarksFile(path_, std::set<GURL>(), false,
                                        L"x", NULL, &parsed_bookmarks, NULL,
                                        NULL);

  // Verify we got back what we wrote.
  ASSERT_EQ(6U, parsed_bookmarks.size());
  // Hardcode the value of IDS_BOOKMARK_BAR_FOLDER_NAME in en-US locale
  // because all the unit tests are run in en-US locale. 
  const wchar_t* kBookmarkBarFolderName = L"Bookmarks bar"; 
  AssertBookmarkEntryEquals(parsed_bookmarks[0], false, url1, url1_title, t1,
                            kBookmarkBarFolderName, f1_title, std::wstring());
  AssertBookmarkEntryEquals(parsed_bookmarks[1], false, url2, url2_title, t2,
                            kBookmarkBarFolderName, f1_title, f2_title);
  AssertBookmarkEntryEquals(parsed_bookmarks[2], false, url3, url3_title, t3,
                            kBookmarkBarFolderName, std::wstring(),
                            std::wstring());
  AssertBookmarkEntryEquals(parsed_bookmarks[3], false, url1, url1_title, t1,
                            std::wstring(), std::wstring(), std::wstring());
  AssertBookmarkEntryEquals(parsed_bookmarks[4], false, url2, url2_title, t2,
                            std::wstring(), std::wstring(), std::wstring());
  AssertBookmarkEntryEquals(parsed_bookmarks[5], false, url1, url1_title, t1,
                            f3_title, f4_title, std::wstring());
}
