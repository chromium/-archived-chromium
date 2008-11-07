// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"

#include <string>
#include <vector>

#include "chrome/browser/importer/importer.h"
#include "chrome/browser/importer/toolbar_importer.h"
#include "chrome/common/libxml_utils.h"


namespace toolbar_importer_unittest {
static const std::wstring kTitle = L"MyTitle";
static const std::wstring kUrl = L"http://www.google.com/";
static const std::wstring kTimestamp = L"1153328691085181";
static const std::wstring kFolder = L"Google";
static const std::wstring kFolder2 = L"Homepage";
static const std::wstring kFolderArray[3] = {L"Google", L"Search", L"Page"};

static const std::wstring kOtherTitle = L"MyOtherTitle";
static const std::wstring kOtherUrl = L"http://www.google.com/mail";
static const std::wstring kOtherFolder = L"Mail";

static const std::string kGoodBookMark =
    " <bookmarks>"
    " <bookmark> "
    "<title>MyTitle</title> "
    "<url>http://www.google.com/</url> "
    "<timestamp>1153328691085181</timestamp> "
    "<id>N123nasdf239</id> <notebook_id>Bxxxxxxx</notebook_id> "
    "<section_id>Sxxxxxx</section_id> <has_highlight>0</has_highlight>"
    "<labels> <label>Google</label> </labels> "
    "<attributes> "
    "<attribute> "
    "<name>favicon_url</name> <value>http://www.google.com/favicon.ico</value> "
    "</attribute> "
    "<attribute> "
    "<name>favicon_timestamp</name> <value>1153328653</value> "
    "</attribute> "
    "<attribute> <name>notebook_name</name> <value>My notebook 0</value> "
    "</attribute> "
    "<attribute> <name>section_name</name> <value>My section 0 "
    "</value> </attribute> </attributes> "
    "</bookmark> </bookmarks>";
static const std::string kGoodBookMarkNoLabel =
    " <bookmarks>"
    " <bookmark> "
    "<title>MyTitle</title> "
    "<url>http://www.google.com/</url> "
    "<timestamp>1153328691085181</timestamp> "
    "<id>N123nasdf239</id> <notebook_id>Bxxxxxxx</notebook_id> "
    "<section_id>Sxxxxxx</section_id> <has_highlight>0</has_highlight>"
    "<labels> </labels> "
    "<attributes> "
    "<attribute> "
    "<name>favicon_url</name> <value>http://www.google.com/favicon.ico</value> "
    "</attribute> "
    "<attribute> "
    "<name>favicon_timestamp</name> <value>1153328653</value> "
    "</attribute> "
    "<attribute> <name>notebook_name</name> <value>My notebook 0</value> "
    "</attribute> "
    "<attribute> <name>section_name</name> <value>My section 0 "
    "</value> </attribute> </attributes> "
    "</bookmark> </bookmarks>";
static const std::string kGoodBookMarkTwoLabels =
    " <bookmarks>"
    " <bookmark> "
    "<title>MyTitle</title> "
    "<url>http://www.google.com/</url> "
    "<timestamp>1153328691085181</timestamp> "
    "<id>N123nasdf239</id> <notebook_id>Bxxxxxxx</notebook_id> "
    "<section_id>Sxxxxxx</section_id> <has_highlight>0</has_highlight>"
    "<labels> <label>Google</label> <label>Homepage</label> </labels> "
    "<attributes> "
    "<attribute> "
    "<name>favicon_url</name> <value>http://www.google.com/favicon.ico</value> "
    "</attribute> "
    "<attribute> "
    "<name>favicon_timestamp</name> <value>1153328653</value> "
    "</attribute> "
    "<attribute> <name>notebook_name</name> <value>My notebook 0</value> "
    "</attribute> "
    "<attribute> <name>section_name</name> <value>My section 0 "
    "</value> </attribute> </attributes> "
    "</bookmark> </bookmarks>";
static const std::string kGoodBookMarkFolderLabel =
    " <bookmarks>"
    " <bookmark> "
    "<title>MyTitle</title> "
    "<url>http://www.google.com/</url> "
    "<timestamp>1153328691085181</timestamp> "
    "<id>N123nasdf239</id> <notebook_id>Bxxxxxxx</notebook_id> "
    "<section_id>Sxxxxxx</section_id> <has_highlight>0</has_highlight>"
    "<labels> <label>Google:Search:Page</label> </labels> "
    "<attributes> "
    "<attribute> "
    "<name>favicon_url</name> <value>http://www.google.com/favicon.ico</value> "
    "</attribute> "
    "<attribute> "
    "<name>favicon_timestamp</name> <value>1153328653</value> "
    "</attribute> "
    "<attribute> <name>notebook_name</name> <value>My notebook 0</value> "
    "</attribute> "
    "<attribute> <name>section_name</name> <value>My section 0 "
    "</value> </attribute> </attributes> "
    "</bookmark> </bookmarks>";
static const std::string kGoodBookMarkNoFavicon =
    " <bookmarks>"
    " <bookmark> "
    "<title>MyTitle</title> "
    "<url>http://www.google.com/</url> "
    "<timestamp>1153328691085181</timestamp> "
    "<id>N123nasdf239</id> <notebook_id>Bxxxxxxx</notebook_id> "
    "<section_id>Sxxxxxx</section_id> <has_highlight>0</has_highlight>"
    "<labels> <label>Google</label> </labels> "
    "<attributes> "
    "<attribute> "
    "<name>favicon_timestamp</name> <value>1153328653</value> "
    "</attribute> "
    "<attribute> <name>notebook_name</name> <value>My notebook 0</value> "
    "</attribute> "
    "<attribute> <name>section_name</name> <value>My section 0 "
    "</value> </attribute> </attributes> "
    "</bookmark> </bookmarks>";
static const std::string kGoodBookMark2Items =
    " <bookmarks>"
    " <bookmark> "
    "<title>MyTitle</title> "
    "<url>http://www.google.com/</url> "
    "<timestamp>1153328691085181</timestamp> "
    "<id>N123nasdf239</id> <notebook_id>Bxxxxxxx</notebook_id> "
    "<section_id>Sxxxxxx</section_id> <has_highlight>0</has_highlight>"
    "<labels> <label>Google</label> </labels> "
    "<attributes> "
    "<attribute> "
    "<name>favicon_url</name> <value>http://www.google.com/favicon.ico</value> "
    "</attribute> "
    "<attribute> "
    "<name>favicon_timestamp</name> <value>1153328653</value> "
    "</attribute> "
    "<attribute> <name>notebook_name</name> <value>My notebook 0</value> "
    "</attribute> "
    "<attribute> <name>section_name</name> <value>My section 0 "
    "</value> </attribute> </attributes> "
    "</bookmark>"
    " <bookmark> "
    "<title>MyOtherTitle</title> "
    "<url>http://www.google.com/mail</url> "
    "<timestamp>1153328691085181</timestamp> "
    "<id>N123nasdf239</id> <notebook_id>Bxxxxxxx</notebook_id> "
    "<section_id>Sxxxxxx</section_id> <has_highlight>0</has_highlight>"
    "<labels> <label>Mail</label> </labels> "
    "<attributes> "
    "<attribute> "
    "<name>favicon_url</name>"
    "<value>http://www.google.com/mail/favicon.ico</value> "
    "</attribute> "
    "<attribute> "
    "<name>favicon_timestamp</name> <value>1253328653</value> "
    "</attribute> "
    "<attribute> <name>notebook_name</name> <value>My notebook 0</value> "
    "</attribute> "
    "<attribute> <name>section_name</name> <value>My section 0 "
    "</value> </attribute> </attributes> "
    "</bookmark>"
    "</bookmarks>";
static const std::string kEmptyString = "";
static const std::string kBadBookMarkNoBookmarks =
    " <bookmark> "
    "<title>MyTitle</title> "
    "<url>http://www.google.com/</url> "
    "<timestamp>1153328691085181</timestamp> "
    "<id>N123nasdf239</id> <notebook_id>Bxxxxxxx</notebook_id> "
    "<section_id>Sxxxxxx</section_id> <has_highlight>0</has_highlight>"
    "<labels> <label>Google</label> </labels> "
    "<attributes> "
    "<attribute> "
    "<name>favicon_url</name> <value>http://www.google.com/favicon.ico</value> "
    "</attribute> "
    "<attribute> "
    "<name>favicon_timestamp</name> <value>1153328653</value> "
    "</attribute> "
    "<attribute> <name>notebook_name</name> <value>My notebook 0</value> "
    "</attribute> "
    "<attribute> <name>section_name</name> <value>My section 0 "
    "</value> </attribute> </attributes> "
    "</bookmark> </bookmarks>";
static const std::string kBadBookMarkNoBookmark =
    " <bookmarks>"
    "<title>MyTitle</title> "
    "<url>http://www.google.com/</url> "
    "<timestamp>1153328691085181</timestamp> "
    "<id>N123nasdf239</id> <notebook_id>Bxxxxxxx</notebook_id> "
    "<section_id>Sxxxxxx</section_id> <has_highlight>0</has_highlight>"
    "<labels> <label>Google</label> </labels> "
    "<attributes> "
    "<attribute> "
    "<name>favicon_url</name> <value>http://www.google.com/favicon.ico</value> "
    "</attribute> "
    "<attribute> "
    "<name>favicon_timestamp</name> <value>1153328653</value> "
    "</attribute> "
    "<attribute> <name>notebook_name</name> <value>My notebook 0</value> "
    "</attribute> "
    "<attribute> <name>section_name</name> <value>My section 0 "
    "</value> </attribute> </attributes> "
    "</bookmark> </bookmarks>";
static const std::string kBadBookMarkNoTitle =
    " <bookmarks>"
    " <bookmark> "
    "<url>http://www.google.com/</url> "
    "<timestamp>1153328691085181</timestamp> "
    "<id>N123nasdf239</id> <notebook_id>Bxxxxxxx</notebook_id> "
    "<section_id>Sxxxxxx</section_id> <has_highlight>0</has_highlight>"
    "<labels> <label>Google</label> </labels> "
    "<attributes> "
    "<attribute> "
    "<name>favicon_url</name> <value>http://www.google.com/favicon.ico</value> "
    "</attribute> "
    "<attribute> "
    "<name>favicon_timestamp</name> <value>1153328653</value> "
    "</attribute> "
    "<attribute> <name>notebook_name</name> <value>My notebook 0</value> "
    "</attribute> "
    "<attribute> <name>section_name</name> <value>My section 0 "
    "</value> </attribute> </attributes> "
    "</bookmark> </bookmarks>";
static const std::string kBadBookMarkNoUrl =
    " <bookmarks>"
    " <bookmark> "
    "<title>MyTitle</title> "
    "<timestamp>1153328691085181</timestamp> "
    "<id>N123nasdf239</id> <notebook_id>Bxxxxxxx</notebook_id> "
    "<section_id>Sxxxxxx</section_id> <has_highlight>0</has_highlight>"
    "<labels> <label>Google</label> </labels> "
    "<attributes> "
    "<attribute> "
    "<name>favicon_url</name> <value>http://www.google.com/favicon.ico</value> "
    "</attribute> "
    "<attribute> "
    "<name>favicon_timestamp</name> <value>1153328653</value> "
    "</attribute> "
    "<attribute> <name>notebook_name</name> <value>My notebook 0</value> "
    "</attribute> "
    "<attribute> <name>section_name</name> <value>My section 0 "
    "</value> </attribute> </attributes> "
    "</bookmark> </bookmarks>";
static const std::string kBadBookMarkNoTimestamp =
    " <bookmarks>"
    " <bookmark> "
    "<title>MyTitle</title> "
    "<url>http://www.google.com/</url> "
    "<id>N123nasdf239</id> <notebook_id>Bxxxxxxx</notebook_id> "
    "<section_id>Sxxxxxx</section_id> <has_highlight>0</has_highlight>"
    "<labels> <label>Google</label> </labels> "
    "<attributes> "
    "<attribute> "
    "<name>favicon_url</name> <value>http://www.google.com/favicon.ico</value> "
    "</attribute> "
    "<attribute> "
    "<name>favicon_timestamp</name> <value>1153328653</value> "
    "</attribute> "
    "<attribute> <name>notebook_name</name> <value>My notebook 0</value> "
    "</attribute> "
    "<attribute> <name>section_name</name> <value>My section 0 "
    "</value> </attribute> </attributes> "
    "</bookmark> </bookmarks>";
static const std::string kBadBookMarkNoLabels =
    " <bookmarks>"
    " <bookmark> "
    "<title>MyTitle</title> "
    "<url>http://www.google.com/</url> "
    "<timestamp>1153328691085181</timestamp> "
    "<id>N123nasdf239</id> <notebook_id>Bxxxxxxx</notebook_id> "
    "<section_id>Sxxxxxx</section_id> <has_highlight>0</has_highlight>"
    "<attributes> "
    "<attribute> "
    "<name>favicon_url</name> <value>http://www.google.com/favicon.ico</value> "
    "</attribute> "
    "<attribute> "
    "<name>favicon_timestamp</name> <value>1153328653</value> "
    "</attribute> "
    "<attribute> <name>notebook_name</name> <value>My notebook 0</value> "
    "</attribute> "
    "<attribute> <name>section_name</name> <value>My section 0 "
    "</value> </attribute> </attributes> "
    "</bookmark> </bookmarks>";
}  // namespace toolbar_importer_unittest

// Since the above is very dense to read I enumerate the test cases here.
// 1. Correct bookmark structure with one label
// 2. "" with no labels
// 3. "" with two labels
// 4. "" with a folder->label translation done by toolbar
// 5. "" with no favicon
// 6. Two correct bookmarks.
// The following are error cases by removing sections:
// 7. Empty string
// 8. No <bookmarks>
// 9. No <bookmark>
// 10. No <title>
// 11. No <url>
// 12. No <timestamp>
// 13. No <labels>
TEST(Toolbar5ImporterTest, BookmarkParse) {
//  bool ParseBookmarksFromReader(
//    XmlReader* reader,
//    std::vector<ProfileWriter::BookmarkEntry>* bookmarks,
//    std::vector< history::ImportedFavIconUsage >* favicons);

  XmlReader reader;
  std::vector<ProfileWriter::BookmarkEntry> bookmarks;

  const GURL url(toolbar_importer_unittest::kUrl);
  const GURL other_url(toolbar_importer_unittest::kOtherUrl);

  // Test case 1 - Basic bookmark, single lables
  bookmarks.clear();
  EXPECT_TRUE(reader.Load(toolbar_importer_unittest::kGoodBookMark));
  EXPECT_TRUE(Toolbar5Importer::ParseBookmarksFromReader(
    &reader,
    &bookmarks));

  // verificaiton
  EXPECT_EQ(bookmarks.size(), 1);
  EXPECT_FALSE(bookmarks[0].in_toolbar);
  EXPECT_EQ(toolbar_importer_unittest::kTitle, bookmarks[0].title);
  EXPECT_EQ(url, bookmarks[0].url);
  EXPECT_EQ(2, bookmarks[0].path.size());
  EXPECT_EQ(toolbar_importer_unittest::kFolder, bookmarks[0].path[1]);

  // Test case 2 - No labels
  bookmarks.clear();
  EXPECT_TRUE(reader.Load(toolbar_importer_unittest::kGoodBookMarkNoLabel));
  EXPECT_TRUE(Toolbar5Importer::ParseBookmarksFromReader(
    &reader,
    &bookmarks));

  // verificaiton
  EXPECT_EQ(bookmarks.size(), 1);
  EXPECT_FALSE(bookmarks[0].in_toolbar);
  EXPECT_EQ(toolbar_importer_unittest::kTitle, bookmarks[0].title);
  EXPECT_EQ(bookmarks[0].url, url);
  EXPECT_EQ(1, bookmarks[0].path.size());

  // Test case 3 - Two labels
  bookmarks.clear();
  EXPECT_TRUE(reader.Load(toolbar_importer_unittest::kGoodBookMarkTwoLabels));
  EXPECT_TRUE(Toolbar5Importer::ParseBookmarksFromReader(
    &reader,
    &bookmarks));

  // verificaiton
  EXPECT_EQ(2, bookmarks.size());
  EXPECT_FALSE(bookmarks[0].in_toolbar);
  EXPECT_FALSE(bookmarks[1].in_toolbar);
  EXPECT_EQ(toolbar_importer_unittest::kTitle, bookmarks[0].title);
  EXPECT_EQ(toolbar_importer_unittest::kTitle, bookmarks[1].title);
  EXPECT_EQ(bookmarks[0].url, url);
  EXPECT_EQ(bookmarks[1].url, url);
  EXPECT_EQ(toolbar_importer_unittest::kFolder, bookmarks[0].path[1]);
  EXPECT_EQ(toolbar_importer_unittest::kFolder2, bookmarks[1].path[1]);

  // Test case 4 - Label with a colon (file name translation).
  bookmarks.clear();
  EXPECT_TRUE(reader.Load(toolbar_importer_unittest::kGoodBookMarkFolderLabel));
  EXPECT_TRUE(Toolbar5Importer::ParseBookmarksFromReader(
    &reader,
    &bookmarks));

  // verificaiton
  EXPECT_EQ(bookmarks.size(), 1);
  EXPECT_FALSE(bookmarks[0].in_toolbar);
  EXPECT_EQ(toolbar_importer_unittest::kTitle, bookmarks[0].title);
  EXPECT_EQ(bookmarks[0].url, url);
  EXPECT_EQ(4, bookmarks[0].path.size());
  EXPECT_TRUE(toolbar_importer_unittest::kFolderArray[0] ==
      bookmarks[0].path[1]);
  EXPECT_TRUE(toolbar_importer_unittest::kFolderArray[1] ==
      bookmarks[0].path[2]);
  EXPECT_TRUE(toolbar_importer_unittest::kFolderArray[2] ==
      bookmarks[0].path[3]);

  // Test case 5 - No favicon
  bookmarks.clear();
  EXPECT_TRUE(reader.Load(toolbar_importer_unittest::kGoodBookMarkNoFavicon));
  EXPECT_TRUE(Toolbar5Importer::ParseBookmarksFromReader(
    &reader,
    &bookmarks));

  // verificaiton
  EXPECT_EQ(1, bookmarks.size());
  EXPECT_FALSE(bookmarks[0].in_toolbar);
  EXPECT_EQ(toolbar_importer_unittest::kTitle, bookmarks[0].title);
  EXPECT_EQ(bookmarks[0].url, url);
  EXPECT_EQ(2, bookmarks[0].path.size());
  EXPECT_EQ(toolbar_importer_unittest::kFolder, bookmarks[0].path[1]);

  // Test case 6 - Two bookmarks
  bookmarks.clear();
  EXPECT_TRUE(reader.Load(toolbar_importer_unittest::kGoodBookMark2Items));
  EXPECT_TRUE(Toolbar5Importer::ParseBookmarksFromReader(
    &reader,
    &bookmarks));

  // verificaiton
  EXPECT_EQ(2, bookmarks.size());
  EXPECT_FALSE(bookmarks[0].in_toolbar);
  EXPECT_FALSE(bookmarks[1].in_toolbar);
  EXPECT_EQ(toolbar_importer_unittest::kTitle, bookmarks[0].title);
  EXPECT_EQ(toolbar_importer_unittest::kOtherTitle, bookmarks[1].title);
  EXPECT_EQ(bookmarks[0].url, url);
  EXPECT_EQ(bookmarks[1].url, other_url);
  EXPECT_EQ(2, bookmarks[0].path.size());
  EXPECT_EQ(toolbar_importer_unittest::kFolder, bookmarks[0].path[1]);
  EXPECT_EQ(2, bookmarks[0].path.size());
  EXPECT_EQ(toolbar_importer_unittest::kOtherFolder, bookmarks[1].path[1]);

  // Test case 7 - Empty string
  bookmarks.clear();

  EXPECT_FALSE(reader.Load(toolbar_importer_unittest::kEmptyString));

  // Test case 8 - No <bookmarks> section.
  bookmarks.clear();
  EXPECT_TRUE(reader.Load(toolbar_importer_unittest::kBadBookMarkNoBookmarks));
  EXPECT_FALSE(Toolbar5Importer::ParseBookmarksFromReader(
    &reader,
    &bookmarks));

  // Test case 9 - No <bookmark> section
  bookmarks.clear();
  EXPECT_TRUE(reader.Load(toolbar_importer_unittest::kBadBookMarkNoBookmark));
  EXPECT_FALSE(Toolbar5Importer::ParseBookmarksFromReader(
    &reader,
    &bookmarks));


  // Test case 10 - No title.
  bookmarks.clear();
  EXPECT_TRUE(reader.Load(toolbar_importer_unittest::kBadBookMarkNoTitle));
  EXPECT_FALSE(Toolbar5Importer::ParseBookmarksFromReader(
    &reader,
    &bookmarks));


  // Test case 11 - No URL.
  bookmarks.clear();
  EXPECT_TRUE(reader.Load(toolbar_importer_unittest::kBadBookMarkNoUrl));
  EXPECT_FALSE(Toolbar5Importer::ParseBookmarksFromReader(
    &reader,
    &bookmarks));

  // Test case 12 - No timestamp.
  bookmarks.clear();
  EXPECT_TRUE(reader.Load(toolbar_importer_unittest::kBadBookMarkNoTimestamp));
  EXPECT_FALSE(Toolbar5Importer::ParseBookmarksFromReader(
    &reader,
    &bookmarks));

  // Test case 13
  bookmarks.clear();
  EXPECT_TRUE(reader.Load(toolbar_importer_unittest::kBadBookMarkNoLabels));
  EXPECT_FALSE(Toolbar5Importer::ParseBookmarksFromReader(
    &reader,
    &bookmarks));
}
