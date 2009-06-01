// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Unit tests for the SafeBrowsing storage system.

#include "base/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/process_util.h"
#include "base/sha2.h"
#include "base/stats_counters.h"
#include "base/string_util.h"
#include "base/time.h"
#include "chrome/browser/safe_browsing/protocol_parser.h"
#include "chrome/browser/safe_browsing/safe_browsing_database.h"
#include "chrome/test/file_test_utils.h"
#include "googleurl/src/gurl.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

using base::Time;

static const FilePath::CharType kBloomSuffix[] =  FILE_PATH_LITERAL(" Bloom");
static const FilePath::CharType kFilterSuffix[] = FILE_PATH_LITERAL(" Filter");
static const FilePath::CharType kFolderPrefix[] =
    FILE_PATH_LITERAL("SafeBrowsingTestDatabase");

namespace {
  SBPrefix Sha256Prefix(const std::string& str) {
    SBPrefix hash;
    base::SHA256HashString(str, &hash, sizeof(hash));
    return hash;
  }

  // Helper function to do an AddDel or SubDel command.
  void DelChunk(SafeBrowsingDatabase* db,
                const std::string& list,
                int chunk_id,
                bool is_sub_del) {
    std::vector<SBChunkDelete>* deletes = new std::vector<SBChunkDelete>;
    SBChunkDelete chunk_delete;
    chunk_delete.list_name = list;
    chunk_delete.is_sub_del = is_sub_del;
    chunk_delete.chunk_del.push_back(ChunkRange(chunk_id));
    deletes->push_back(chunk_delete);
    db->DeleteChunks(deletes);
  }

  void AddDelChunk(SafeBrowsingDatabase* db,
                   const std::string& list,
                   int chunk_id) {
    DelChunk(db, list, chunk_id, false);
  }

  void SubDelChunk(SafeBrowsingDatabase* db,
                   const std::string& list,
                   int chunk_id) {
    DelChunk(db, list, chunk_id, true);
  }

  // Creates a new test directory.
  FilePath CreateTestDirectory() {
    FilePath temp_dir;
    EXPECT_TRUE(file_util::CreateNewTempDirectory(kFolderPrefix, &temp_dir));
    return temp_dir;
  }

  // Common database test set up code.
  FilePath GetTestDatabaseName(const FilePath& test_dir) {
    FilePath filename(test_dir);
    filename = filename.AppendASCII("SafeBrowsingTestDatabase");
    return filename;
  }

  SafeBrowsingDatabase* SetupTestDatabase(const FilePath& test_dir) {
    FilePath filename = GetTestDatabaseName(test_dir);

    // In case it existed from a previous run.
    file_util::Delete(FilePath(filename.value() + kBloomSuffix), false);
    file_util::Delete(filename, false);

    SafeBrowsingDatabase* database = SafeBrowsingDatabase::Create();
    database->SetSynchronous();
    EXPECT_TRUE(database->Init(filename, NULL));

    return database;
  }

  void TearDownTestDatabase(SafeBrowsingDatabase* database) {
    FilePath filename = database->filename();
    delete database;
    file_util::Delete(filename, false);
    file_util::Delete(FilePath(filename.value() + kFilterSuffix),
                      false);
  }

  void GetListsInfo(SafeBrowsingDatabase* database,
                    std::vector<SBListChunkRanges>* lists) {
    EXPECT_TRUE(database->UpdateStarted());
    database->GetListsInfo(lists);
    database->UpdateFinished(true);
  }

}  // namespace

class SafeBrowsingDatabasePlatformTest : public PlatformTest {
};

// Tests retrieving list name information.
TEST_F(SafeBrowsingDatabasePlatformTest, ListName) {
  FileAutoDeleter file_deleter(CreateTestDirectory());
  SafeBrowsingDatabase* database = SetupTestDatabase(file_deleter.path());

  // Insert some malware add chunks.
  SBChunkHost host;
  host.host = Sha256Prefix("www.evil.com/");
  host.entry = SBEntry::Create(SBEntry::ADD_PREFIX, 1);
  host.entry->set_chunk_id(1);
  host.entry->SetPrefixAt(0, Sha256Prefix("www.evil.com/malware.html"));
  SBChunk chunk;
  chunk.chunk_number = 1;
  chunk.is_add = true;
  chunk.hosts.push_back(host);
  std::deque<SBChunk>* chunks = new std::deque<SBChunk>;
  chunks->push_back(chunk);
  database->UpdateStarted();
  database->InsertChunks(safe_browsing_util::kMalwareList, chunks);

  host.host = Sha256Prefix("www.foo.com/");
  host.entry = SBEntry::Create(SBEntry::ADD_PREFIX, 1);
  host.entry->set_chunk_id(2);
  host.entry->SetPrefixAt(0, Sha256Prefix("www.foo.com/malware.html"));
  chunk.chunk_number = 2;
  chunk.is_add = true;
  chunk.hosts.clear();
  chunk.hosts.push_back(host);
  chunks = new std::deque<SBChunk>;
  chunks->push_back(chunk);
  database->InsertChunks(safe_browsing_util::kMalwareList, chunks);

  host.host = Sha256Prefix("www.whatever.com/");
  host.entry = SBEntry::Create(SBEntry::ADD_PREFIX, 1);
  host.entry->set_chunk_id(3);
  host.entry->SetPrefixAt(0, Sha256Prefix("www.whatever.com/malware.html"));
  chunk.chunk_number = 3;
  chunk.is_add = true;
  chunk.hosts.clear();
  chunk.hosts.push_back(host);
  chunks = new std::deque<SBChunk>;
  chunks->push_back(chunk);
  database->InsertChunks(safe_browsing_util::kMalwareList, chunks);
  database->UpdateFinished(true);

  std::vector<SBListChunkRanges> lists;
  GetListsInfo(database, &lists);
  EXPECT_TRUE(lists[0].name == safe_browsing_util::kMalwareList);
  EXPECT_EQ(lists[0].adds, "1-3");
  EXPECT_TRUE(lists[0].subs.empty());
  lists.clear();

  // Insert a malware sub chunk.
  host.host = Sha256Prefix("www.subbed.com/");
  host.entry = SBEntry::Create(SBEntry::SUB_PREFIX, 1);
  host.entry->set_chunk_id(7);
  host.entry->SetChunkIdAtPrefix(0, 19);
  host.entry->SetPrefixAt(0, Sha256Prefix("www.subbed.com/notevil1.html"));
  chunk.chunk_number = 7;
  chunk.is_add = false;
  chunk.hosts.clear();
  chunk.hosts.push_back(host);
  chunks = new std::deque<SBChunk>;
  chunks->push_back(chunk);

  database->UpdateStarted();
  database->GetListsInfo(&lists);
  database->InsertChunks(safe_browsing_util::kMalwareList, chunks);
  database->UpdateFinished(true);
  lists.clear();

  GetListsInfo(database, &lists);
  EXPECT_TRUE(lists[0].name == safe_browsing_util::kMalwareList);
  EXPECT_EQ(lists[0].adds, "1-3");
  EXPECT_EQ(lists[0].subs, "7");
  if (lists.size() == 2) {
    // Old style database won't have the second entry since it creates the lists
    // when it receives an update containing that list. The new bloom filter
    // based database has these values hard coded.
    EXPECT_TRUE(lists[1].name == safe_browsing_util::kPhishingList);
    EXPECT_TRUE(lists[1].adds.empty());
    EXPECT_TRUE(lists[1].subs.empty());
  }
  lists.clear();

  // Add a phishing add chunk.
  host.host = Sha256Prefix("www.evil.com/");
  host.entry = SBEntry::Create(SBEntry::ADD_PREFIX, 1);
  host.entry->set_chunk_id(47);
  host.entry->SetPrefixAt(0, Sha256Prefix("www.evil.com/phishing.html"));
  chunk.chunk_number = 47;
  chunk.is_add = true;
  chunk.hosts.clear();
  chunk.hosts.push_back(host);
  chunks = new std::deque<SBChunk>;
  chunks->push_back(chunk);
  database->UpdateStarted();
  database->GetListsInfo(&lists);
  database->InsertChunks(safe_browsing_util::kPhishingList, chunks);

  // Insert some phishing sub chunks.
  host.host = Sha256Prefix("www.phishy.com/");
  host.entry = SBEntry::Create(SBEntry::SUB_PREFIX, 1);
  host.entry->set_chunk_id(200);
  host.entry->SetChunkIdAtPrefix(0, 1999);
  host.entry->SetPrefixAt(0, Sha256Prefix("www.phishy.com/notevil1.html"));
  chunk.chunk_number = 200;
  chunk.is_add = false;
  chunk.hosts.clear();
  chunk.hosts.push_back(host);
  chunks = new std::deque<SBChunk>;
  chunks->push_back(chunk);
  database->InsertChunks(safe_browsing_util::kPhishingList, chunks);

  host.host = Sha256Prefix("www.phishy2.com/");
  host.entry = SBEntry::Create(SBEntry::SUB_PREFIX, 1);
  host.entry->set_chunk_id(201);
  host.entry->SetChunkIdAtPrefix(0, 1999);
  host.entry->SetPrefixAt(0, Sha256Prefix("www.phishy2.com/notevil1.html"));
  chunk.chunk_number = 201;
  chunk.is_add = false;
  chunk.hosts.clear();
  chunk.hosts.push_back(host);
  chunks = new std::deque<SBChunk>;
  chunks->push_back(chunk);
  database->InsertChunks(safe_browsing_util::kPhishingList, chunks);
  database->UpdateFinished(true);
  lists.clear();

  GetListsInfo(database, &lists);
  EXPECT_TRUE(lists[0].name == safe_browsing_util::kMalwareList);
  EXPECT_EQ(lists[0].adds, "1-3");
  EXPECT_EQ(lists[0].subs, "7");
  EXPECT_TRUE(lists[1].name == safe_browsing_util::kPhishingList);
  EXPECT_EQ(lists[1].adds, "47");
  EXPECT_EQ(lists[1].subs, "200-201");
  lists.clear();

  TearDownTestDatabase(database);
}

// Checks database reading and writing.
TEST(SafeBrowsingDatabase, Database) {
  FileAutoDeleter file_deleter(CreateTestDirectory());
  SafeBrowsingDatabase* database = SetupTestDatabase(file_deleter.path());

  // Add a simple chunk with one hostkey.
  SBChunkHost host;
  host.host = Sha256Prefix("www.evil.com/");
  host.entry = SBEntry::Create(SBEntry::ADD_PREFIX, 2);
  host.entry->set_chunk_id(1);
  host.entry->SetPrefixAt(0, Sha256Prefix("www.evil.com/phishing.html"));
  host.entry->SetPrefixAt(1, Sha256Prefix("www.evil.com/malware.html"));

  SBChunk chunk;
  chunk.chunk_number = 1;
  chunk.is_add = true;
  chunk.hosts.push_back(host);

  std::deque<SBChunk>* chunks = new std::deque<SBChunk>;
  chunks->push_back(chunk);
  std::vector<SBListChunkRanges> lists;
  database->UpdateStarted();
  database->GetListsInfo(&lists);
  database->InsertChunks(safe_browsing_util::kMalwareList, chunks);

  // Add another chunk with two different hostkeys.
  host.host = Sha256Prefix("www.evil.com/");
  host.entry = SBEntry::Create(SBEntry::ADD_PREFIX, 2);
  host.entry->set_chunk_id(2);
  host.entry->SetPrefixAt(0, Sha256Prefix("www.evil.com/notevil1.html"));
  host.entry->SetPrefixAt(1, Sha256Prefix("www.evil.com/notevil2.html"));

  chunk.chunk_number = 2;
  chunk.hosts.clear();
  chunk.hosts.push_back(host);

  host.host = Sha256Prefix("www.good.com/");
  host.entry = SBEntry::Create(SBEntry::ADD_PREFIX, 2);
  host.entry->set_chunk_id(2);
  host.entry->SetPrefixAt(0, Sha256Prefix("www.good.com/good1.html"));
  host.entry->SetPrefixAt(1, Sha256Prefix("www.good.com/good2.html"));

  chunk.hosts.push_back(host);

  chunks = new std::deque<SBChunk>;
  chunks->push_back(chunk);

  database->InsertChunks(safe_browsing_util::kMalwareList, chunks);

  // and a chunk with an IP-based host
  host.host = Sha256Prefix("192.168.0.1/");
  host.entry = SBEntry::Create(SBEntry::ADD_PREFIX, 1);
  host.entry->set_chunk_id(3);
  host.entry->SetPrefixAt(0, Sha256Prefix("192.168.0.1/malware.html"));

  chunk.chunk_number = 3;
  chunk.hosts.clear();
  chunk.hosts.push_back(host);

  chunks = new std::deque<SBChunk>;
  chunks->push_back(chunk);
  database->InsertChunks(safe_browsing_util::kMalwareList, chunks);
  database->UpdateFinished(true);
  lists.clear();

  // Make sure they were added correctly.
  GetListsInfo(database, &lists);
  EXPECT_TRUE(lists[0].name == safe_browsing_util::kMalwareList);
  EXPECT_EQ(lists[0].adds, "1-3");
  EXPECT_TRUE(lists[0].subs.empty());
  lists.clear();

  const Time now = Time::Now();
  std::vector<SBFullHashResult> full_hashes;
  std::vector<SBPrefix> prefix_hits;
  std::string matching_list;
  EXPECT_TRUE(database->ContainsUrl(GURL("http://www.evil.com/phishing.html"),
                                    &matching_list, &prefix_hits,
                                    &full_hashes, now));
  EXPECT_EQ(prefix_hits[0], Sha256Prefix("www.evil.com/phishing.html"));
  EXPECT_EQ(prefix_hits.size(), 1U);

  EXPECT_TRUE(database->ContainsUrl(GURL("http://www.evil.com/malware.html"),
                                    &matching_list, &prefix_hits,
                                    &full_hashes, now));

  EXPECT_TRUE(database->ContainsUrl(GURL("http://www.evil.com/notevil1.html"),
                                    &matching_list, &prefix_hits,
                                    &full_hashes, now));

  EXPECT_TRUE(database->ContainsUrl(GURL("http://www.evil.com/notevil2.html"),
                                    &matching_list, &prefix_hits,
                                    &full_hashes, now));

  EXPECT_TRUE(database->ContainsUrl(GURL("http://www.good.com/good1.html"),
                                    &matching_list, &prefix_hits,
                                    &full_hashes, now));

  EXPECT_TRUE(database->ContainsUrl(GURL("http://www.good.com/good2.html"),
                                    &matching_list, &prefix_hits,
                                    &full_hashes, now));

  EXPECT_TRUE(database->ContainsUrl(GURL("http://192.168.0.1/malware.html"),
                                    &matching_list, &prefix_hits,
                                    &full_hashes, now));

  EXPECT_FALSE(database->ContainsUrl(GURL("http://www.evil.com/"),
                                     &matching_list, &prefix_hits,
                                     &full_hashes, now));
  EXPECT_EQ(prefix_hits.size(), 0U);

  EXPECT_FALSE(database->ContainsUrl(GURL("http://www.evil.com/robots.txt"),
                                     &matching_list, &prefix_hits,
                                     &full_hashes, now));



  // Attempt to re-add the first chunk (should be a no-op).
  // see bug: http://code.google.com/p/chromium/issues/detail?id=4522
  host.host = Sha256Prefix("www.evil.com/");
  host.entry = SBEntry::Create(SBEntry::ADD_PREFIX, 2);
  host.entry->set_chunk_id(1);
  host.entry->SetPrefixAt(0, Sha256Prefix("www.evil.com/phishing.html"));
  host.entry->SetPrefixAt(1, Sha256Prefix("www.evil.com/malware.html"));

  chunk.chunk_number = 1;
  chunk.is_add = true;
  chunk.hosts.clear();
  chunk.hosts.push_back(host);

  chunks = new std::deque<SBChunk>;
  chunks->push_back(chunk);
  database->UpdateStarted();
  database->GetListsInfo(&lists);
  database->InsertChunks(safe_browsing_util::kMalwareList, chunks);
  database->UpdateFinished(true);
  lists.clear();

  GetListsInfo(database, &lists);
  EXPECT_TRUE(lists[0].name == safe_browsing_util::kMalwareList);
  EXPECT_EQ(lists[0].adds, "1-3");
  EXPECT_TRUE(lists[0].subs.empty());
  lists.clear();


  // Test removing a single prefix from the add chunk.
  host.host = Sha256Prefix("www.evil.com/");
  host.entry = SBEntry::Create(SBEntry::SUB_PREFIX, 1);
  host.entry->set_chunk_id(2);
  host.entry->SetChunkIdAtPrefix(0, 2);
  host.entry->SetPrefixAt(0, Sha256Prefix("www.evil.com/notevil1.html"));

  chunk.is_add = false;
  chunk.chunk_number = 4;
  chunk.hosts.clear();
  chunk.hosts.push_back(host);

  chunks = new std::deque<SBChunk>;
  chunks->push_back(chunk);

  database->UpdateStarted();
  database->GetListsInfo(&lists);
  database->InsertChunks(safe_browsing_util::kMalwareList, chunks);
  database->UpdateFinished(true);
  lists.clear();

  EXPECT_TRUE(database->ContainsUrl(GURL("http://www.evil.com/phishing.html"),
                                    &matching_list, &prefix_hits,
                                    &full_hashes, now));
  EXPECT_EQ(prefix_hits[0], Sha256Prefix("www.evil.com/phishing.html"));
  EXPECT_EQ(prefix_hits.size(), 1U);

  EXPECT_FALSE(database->ContainsUrl(GURL("http://www.evil.com/notevil1.html"),
                                     &matching_list, &prefix_hits,
                                     &full_hashes, now));
  EXPECT_EQ(prefix_hits.size(), 0U);

  EXPECT_TRUE(database->ContainsUrl(GURL("http://www.evil.com/notevil2.html"),
                                    &matching_list, &prefix_hits,
                                    &full_hashes, now));

  EXPECT_TRUE(database->ContainsUrl(GURL("http://www.good.com/good1.html"),
                                    &matching_list, &prefix_hits,
                                    &full_hashes, now));

  EXPECT_TRUE(database->ContainsUrl(GURL("http://www.good.com/good2.html"),
                                    &matching_list, &prefix_hits,
                                    &full_hashes, now));

  GetListsInfo(database, &lists);
  EXPECT_TRUE(lists[0].name == safe_browsing_util::kMalwareList);
  EXPECT_EQ(lists[0].subs, "4");
  lists.clear();

  // Test the same sub chunk again.  This should be a no-op.
  // see bug: http://code.google.com/p/chromium/issues/detail?id=4522
  host.host = Sha256Prefix("www.evil.com/");
  host.entry = SBEntry::Create(SBEntry::SUB_PREFIX, 1);
  host.entry->set_chunk_id(2);
  host.entry->SetChunkIdAtPrefix(0, 2);
  host.entry->SetPrefixAt(0, Sha256Prefix("www.evil.com/notevil1.html"));

  chunk.is_add = false;
  chunk.chunk_number = 4;
  chunk.hosts.clear();
  chunk.hosts.push_back(host);

  chunks = new std::deque<SBChunk>;
  chunks->push_back(chunk);

  database->UpdateStarted();
  database->GetListsInfo(&lists);
  database->InsertChunks(safe_browsing_util::kMalwareList, chunks);
  database->UpdateFinished(true);
  lists.clear();

  GetListsInfo(database, &lists);
  EXPECT_TRUE(lists[0].name == safe_browsing_util::kMalwareList);
  EXPECT_EQ(lists[0].subs, "4");
  lists.clear();


  // Test removing all the prefixes from an add chunk.
  database->UpdateStarted();
  database->GetListsInfo(&lists);
  AddDelChunk(database, safe_browsing_util::kMalwareList, 2);
  database->UpdateFinished(true);
  lists.clear();

  EXPECT_FALSE(database->ContainsUrl(GURL("http://www.evil.com/notevil2.html"),
                                     &matching_list, &prefix_hits,
                                     &full_hashes, now));

  EXPECT_FALSE(database->ContainsUrl(GURL("http://www.good.com/good1.html"),
                                     &matching_list, &prefix_hits,
                                     &full_hashes, now));

  EXPECT_FALSE(database->ContainsUrl(GURL("http://www.good.com/good2.html"),
                                     &matching_list, &prefix_hits,
                                     &full_hashes, now));

  GetListsInfo(database, &lists);
  EXPECT_TRUE(lists[0].name == safe_browsing_util::kMalwareList);
  EXPECT_EQ(lists[0].adds, "1,3");
  EXPECT_EQ(lists[0].subs, "4");
  lists.clear();

  // The adddel command exposed a bug in the transaction code where any
  // transaction after it would fail.  Add a dummy entry and remove it to
  // make sure the transcation works fine.
  host.host = Sha256Prefix("www.redherring.com/");
  host.entry = SBEntry::Create(SBEntry::ADD_PREFIX, 1);
  host.entry->set_chunk_id(1);
  host.entry->SetPrefixAt(0, Sha256Prefix("www.redherring.com/index.html"));

  chunk.is_add = true;
  chunk.chunk_number = 44;
  chunk.hosts.clear();
  chunk.hosts.push_back(host);

  chunks = new std::deque<SBChunk>;
  chunks->push_back(chunk);
  database->UpdateStarted();
  database->GetListsInfo(&lists);
  database->InsertChunks(safe_browsing_util::kMalwareList, chunks);

  // Now remove the dummy entry.  If there are any problems with the
  // transactions, asserts will fire.
  AddDelChunk(database, safe_browsing_util::kMalwareList, 44);

  // Test the subdel command.
  SubDelChunk(database, safe_browsing_util::kMalwareList, 4);
  database->UpdateFinished(true);
  lists.clear();

  GetListsInfo(database, &lists);
  EXPECT_TRUE(lists[0].name == safe_browsing_util::kMalwareList);
  EXPECT_EQ(lists[0].adds, "1,3");
  EXPECT_EQ(lists[0].subs, "");
  lists.clear();

  // Test a sub command coming in before the add.
  host.host = Sha256Prefix("www.notevilanymore.com/");
  host.entry = SBEntry::Create(SBEntry::SUB_PREFIX, 2);
  host.entry->set_chunk_id(10);
  host.entry->SetPrefixAt(0, Sha256Prefix("www.notevilanymore.com/index.html"));
  host.entry->SetChunkIdAtPrefix(0, 10);
  host.entry->SetPrefixAt(1, Sha256Prefix("www.notevilanymore.com/good.html"));
  host.entry->SetChunkIdAtPrefix(1, 10);

  chunk.is_add = false;
  chunk.chunk_number = 5;
  chunk.hosts.clear();
  chunk.hosts.push_back(host);

  chunks = new std::deque<SBChunk>;
  chunks->push_back(chunk);
  database->UpdateStarted();
  database->GetListsInfo(&lists);
  database->InsertChunks(safe_browsing_util::kMalwareList, chunks);
  database->UpdateFinished(true);
  lists.clear();

  EXPECT_FALSE(database->ContainsUrl(
      GURL("http://www.notevilanymore.com/index.html"),
      &matching_list, &prefix_hits, &full_hashes, now));

  // Now insert the tardy add chunk.
  host.host = Sha256Prefix("www.notevilanymore.com/");
  host.entry = SBEntry::Create(SBEntry::ADD_PREFIX, 2);
  host.entry->SetPrefixAt(0, Sha256Prefix("www.notevilanymore.com/index.html"));
  host.entry->SetPrefixAt(1, Sha256Prefix("www.notevilanymore.com/good.html"));

  chunk.is_add = true;
  chunk.chunk_number = 10;
  chunk.hosts.clear();
  chunk.hosts.push_back(host);

  chunks = new std::deque<SBChunk>;
  chunks->push_back(chunk);
  database->UpdateStarted();
  database->GetListsInfo(&lists);
  database->InsertChunks(safe_browsing_util::kMalwareList, chunks);
  database->UpdateFinished(true);
  lists.clear();

  EXPECT_FALSE(database->ContainsUrl(
      GURL("http://www.notevilanymore.com/index.html"),
      &matching_list, &prefix_hits, &full_hashes, now));

  EXPECT_FALSE(database->ContainsUrl(
      GURL("http://www.notevilanymore.com/good.html"),
      &matching_list, &prefix_hits, &full_hashes, now));

  TearDownTestDatabase(database);
}


// Test adding zero length chunks to the database.
TEST(SafeBrowsingDatabase, ZeroSizeChunk) {
  FileAutoDeleter file_deleter(CreateTestDirectory());
  SafeBrowsingDatabase* database = SetupTestDatabase(file_deleter.path());

  // Populate with a couple of normal chunks.
  SBChunkHost host;
  host.host = Sha256Prefix("www.test.com/");
  host.entry = SBEntry::Create(SBEntry::ADD_PREFIX, 2);
  host.entry->SetPrefixAt(0, Sha256Prefix("www.test.com/test1.html"));
  host.entry->SetPrefixAt(1, Sha256Prefix("www.test.com/test2.html"));
  host.entry->set_chunk_id(1);

  SBChunk chunk;
  chunk.is_add = true;
  chunk.chunk_number = 1;
  chunk.hosts.push_back(host);

  std::deque<SBChunk>* chunks = new std::deque<SBChunk>;
  chunks->push_back(chunk);

  host.host = Sha256Prefix("www.random.com/");
  host.entry = SBEntry::Create(SBEntry::ADD_PREFIX, 2);
  host.entry->SetPrefixAt(0, Sha256Prefix("www.random.com/random1.html"));
  host.entry->SetPrefixAt(1, Sha256Prefix("www.random.com/random2.html"));
  host.entry->set_chunk_id(10);
  chunk.chunk_number = 10;
  chunk.hosts.clear();
  chunk.hosts.push_back(host);
  chunks->push_back(chunk);

  std::vector<SBListChunkRanges> lists;
  database->UpdateStarted();
  database->GetListsInfo(&lists);
  database->InsertChunks(safe_browsing_util::kMalwareList, chunks);
  database->UpdateFinished(true);
  lists.clear();

  // Add an empty ADD and SUB chunk.
  GetListsInfo(database, &lists);
  EXPECT_EQ(lists[0].adds, "1,10");
  lists.clear();

  SBChunk empty_chunk;
  empty_chunk.chunk_number = 19;
  empty_chunk.is_add = true;
  chunks = new std::deque<SBChunk>;
  chunks->push_back(empty_chunk);
  database->UpdateStarted();
  database->GetListsInfo(&lists);
  database->InsertChunks(safe_browsing_util::kMalwareList, chunks);
  chunks = new std::deque<SBChunk>;
  empty_chunk.chunk_number = 7;
  empty_chunk.is_add = false;
  chunks->push_back(empty_chunk);
  database->InsertChunks(safe_browsing_util::kMalwareList, chunks);
  database->UpdateFinished(true);
  lists.clear();

  GetListsInfo(database, &lists);
  EXPECT_EQ(lists[0].adds, "1,10,19");
  EXPECT_EQ(lists[0].subs, "7");
  lists.clear();

  // Add an empty chunk along with a couple that contain data. This should
  // result in the chunk range being reduced in size.
  host.host = Sha256Prefix("www.notempty.com/");
  host.entry = SBEntry::Create(SBEntry::ADD_PREFIX, 1);
  host.entry->SetPrefixAt(0, Sha256Prefix("www.notempty.com/full1.html"));
  host.entry->set_chunk_id(20);
  empty_chunk.chunk_number = 20;
  empty_chunk.is_add = true;
  empty_chunk.hosts.clear();
  empty_chunk.hosts.push_back(host);
  chunks = new std::deque<SBChunk>;
  chunks->push_back(empty_chunk);

  empty_chunk.chunk_number = 21;
  empty_chunk.is_add = true;
  empty_chunk.hosts.clear();
  chunks->push_back(empty_chunk);

  host.host = Sha256Prefix("www.notempty.com/");
  host.entry = SBEntry::Create(SBEntry::ADD_PREFIX, 1);
  host.entry->SetPrefixAt(0, Sha256Prefix("www.notempty.com/full2.html"));
  host.entry->set_chunk_id(22);
  empty_chunk.hosts.clear();
  empty_chunk.hosts.push_back(host);
  empty_chunk.chunk_number = 22;
  empty_chunk.is_add = true;
  chunks->push_back(empty_chunk);

  database->UpdateStarted();
  database->GetListsInfo(&lists);
  database->InsertChunks(safe_browsing_util::kMalwareList, chunks);
  database->UpdateFinished(true);
  lists.clear();

  const Time now = Time::Now();
  std::vector<SBFullHashResult> full_hashes;
  std::vector<SBPrefix> prefix_hits;
  std::string matching_list;
  EXPECT_TRUE(database->ContainsUrl(GURL("http://www.notempty.com/full1.html"),
                                    &matching_list, &prefix_hits,
                                    &full_hashes, now));
  EXPECT_TRUE(database->ContainsUrl(GURL("http://www.notempty.com/full2.html"),
                                    &matching_list, &prefix_hits,
                                    &full_hashes, now));

  GetListsInfo(database, &lists);
  EXPECT_EQ(lists[0].adds, "1,10,19-22");
  EXPECT_EQ(lists[0].subs, "7");
  lists.clear();

  // Handle AddDel and SubDel commands for empty chunks.
  database->UpdateStarted();
  database->GetListsInfo(&lists);
  AddDelChunk(database, safe_browsing_util::kMalwareList, 21);
  database->UpdateFinished(true);
  lists.clear();

  GetListsInfo(database, &lists);
  EXPECT_EQ(lists[0].adds, "1,10,19-20,22");
  EXPECT_EQ(lists[0].subs, "7");
  lists.clear();

  database->UpdateStarted();
  database->GetListsInfo(&lists);
  SubDelChunk(database, safe_browsing_util::kMalwareList, 7);
  database->UpdateFinished(true);
  lists.clear();

  GetListsInfo(database, &lists);
  EXPECT_EQ(lists[0].adds, "1,10,19-20,22");
  EXPECT_EQ(lists[0].subs, "");
  lists.clear();

  TearDownTestDatabase(database);
}

// Utility function for setting up the database for the caching test.
void PopulateDatabaseForCacheTest(SafeBrowsingDatabase* database) {
  // Add a simple chunk with one hostkey and cache it.
  SBChunkHost host;
  host.host = Sha256Prefix("www.evil.com/");
  host.entry = SBEntry::Create(SBEntry::ADD_PREFIX, 2);
  host.entry->set_chunk_id(1);
  host.entry->SetPrefixAt(0, Sha256Prefix("www.evil.com/phishing.html"));
  host.entry->SetPrefixAt(1, Sha256Prefix("www.evil.com/malware.html"));

  SBChunk chunk;
  chunk.chunk_number = 1;
  chunk.is_add = true;
  chunk.hosts.push_back(host);

  std::deque<SBChunk>* chunks = new std::deque<SBChunk>;
  std::vector<SBListChunkRanges> lists;
  chunks->push_back(chunk);
  database->UpdateStarted();
  database->GetListsInfo(&lists);
  database->InsertChunks(safe_browsing_util::kMalwareList, chunks);
  database->UpdateFinished(true);
  lists.clear();

  // Add the GetHash results to the cache.
  SBFullHashResult full_hash;
  base::SHA256HashString("www.evil.com/phishing.html",
                         &full_hash.hash, sizeof(SBFullHash));
  full_hash.list_name = safe_browsing_util::kMalwareList;
  full_hash.add_chunk_id = 1;

  std::vector<SBFullHashResult> results;
  results.push_back(full_hash);

  base::SHA256HashString("www.evil.com/malware.html",
                         &full_hash.hash, sizeof(SBFullHash));
  results.push_back(full_hash);

  std::vector<SBPrefix> prefixes;
  database->CacheHashResults(prefixes, results);
}

TEST(SafeBrowsingDatabase, HashCaching) {
  FileAutoDeleter file_deleter(CreateTestDirectory());
  SafeBrowsingDatabase* database = SetupTestDatabase(file_deleter.path());

  PopulateDatabaseForCacheTest(database);

  // We should have both full hashes in the cache.
  SafeBrowsingDatabase::HashCache* hash_cache = database->hash_cache();
  EXPECT_EQ(hash_cache->size(), 2U);

  // Test the cache lookup for the first prefix.
  std::string listname;
  std::vector<SBPrefix> prefixes;
  std::vector<SBFullHashResult> full_hashes;
  database->ContainsUrl(GURL("http://www.evil.com/phishing.html"),
                        &listname, &prefixes, &full_hashes, Time::Now());
  EXPECT_EQ(full_hashes.size(), 1U);

  SBFullHashResult full_hash;
  base::SHA256HashString("www.evil.com/phishing.html",
                         &full_hash.hash, sizeof(SBFullHash));
  EXPECT_EQ(memcmp(&full_hashes[0].hash,
                   &full_hash.hash, sizeof(SBFullHash)), 0);

  prefixes.clear();
  full_hashes.clear();

  // Test the cache lookup for the second prefix.
  database->ContainsUrl(GURL("http://www.evil.com/malware.html"),
                        &listname, &prefixes, &full_hashes, Time::Now());
  EXPECT_EQ(full_hashes.size(), 1U);
  base::SHA256HashString("www.evil.com/malware.html",
                         &full_hash.hash, sizeof(SBFullHash));
  EXPECT_EQ(memcmp(&full_hashes[0].hash,
                   &full_hash.hash, sizeof(SBFullHash)), 0);

  prefixes.clear();
  full_hashes.clear();

  // Test removing a prefix via a sub chunk.
  SBChunkHost host;
  host.host = Sha256Prefix("www.evil.com/");
  host.entry = SBEntry::Create(SBEntry::SUB_PREFIX, 1);
  host.entry->set_chunk_id(1);
  host.entry->SetChunkIdAtPrefix(0, 1);
  host.entry->SetPrefixAt(0, Sha256Prefix("www.evil.com/phishing.html"));

  SBChunk chunk;
  chunk.chunk_number = 2;
  chunk.is_add = false;
  chunk.hosts.clear();
  chunk.hosts.push_back(host);
  std::deque<SBChunk>* chunks = new std::deque<SBChunk>;
  chunks->push_back(chunk);

  std::vector<SBListChunkRanges> lists;
  database->UpdateStarted();
  database->GetListsInfo(&lists);
  database->InsertChunks(safe_browsing_util::kMalwareList, chunks);
  database->UpdateFinished(true);
  lists.clear();

  // This prefix should still be there.
  database->ContainsUrl(GURL("http://www.evil.com/malware.html"),
                        &listname, &prefixes, &full_hashes, Time::Now());
  EXPECT_EQ(full_hashes.size(), 1U);
  base::SHA256HashString("www.evil.com/malware.html",
                         &full_hash.hash, sizeof(SBFullHash));
  EXPECT_EQ(memcmp(&full_hashes[0].hash,
                   &full_hash.hash, sizeof(SBFullHash)), 0);

  prefixes.clear();
  full_hashes.clear();

  // This prefix should be gone.
  database->ContainsUrl(GURL("http://www.evil.com/phishing.html"),
                        &listname, &prefixes, &full_hashes, Time::Now());
  EXPECT_EQ(full_hashes.size(), 0U);

  prefixes.clear();
  full_hashes.clear();

  // Test that an AddDel for the original chunk removes the last cached entry.
  database->UpdateStarted();
  database->GetListsInfo(&lists);
  AddDelChunk(database, safe_browsing_util::kMalwareList, 1);
  database->UpdateFinished(true);
  database->ContainsUrl(GURL("http://www.evil.com/malware.html"),
                        &listname, &prefixes, &full_hashes, Time::Now());
  EXPECT_EQ(full_hashes.size(), 0U);
  EXPECT_EQ(database->hash_cache()->size(), 0U);

  lists.clear();
  prefixes.clear();
  full_hashes.clear();

  // Test that the cache won't return expired values. First we have to adjust
  // the cached entries' received time to make them older, since the database
  // cache insert uses Time::Now(). First, store some entries.
  PopulateDatabaseForCacheTest(database);
  hash_cache = database->hash_cache();
  EXPECT_EQ(hash_cache->size(), 2U);

  // Now adjust one of the entries times to be in the past.
  base::Time expired = base::Time::Now() - base::TimeDelta::FromMinutes(60);
  SBPrefix key;
  memcpy(&key, &full_hash.hash, sizeof(SBPrefix));
  SafeBrowsingDatabase::HashList& entries = (*hash_cache)[key];
  SafeBrowsingDatabase::HashCacheEntry entry = entries.front();
  entries.pop_front();
  entry.received = expired;
  entries.push_back(entry);

  database->ContainsUrl(GURL("http://www.evil.com/malware.html"),
                        &listname, &prefixes, &full_hashes, expired);
  EXPECT_EQ(full_hashes.size(), 0U);

  // This entry should still exist.
  database->ContainsUrl(GURL("http://www.evil.com/phishing.html"),
                        &listname, &prefixes, &full_hashes, expired);
  EXPECT_EQ(full_hashes.size(), 1U);


  // Testing prefix miss caching. First, we clear out the existing database,
  // Since PopulateDatabaseForCacheTest() doesn't handle adding duplicate
  // chunks.
  database->UpdateStarted();
  database->GetListsInfo(&lists);
  AddDelChunk(database, safe_browsing_util::kMalwareList, 1);
  database->UpdateFinished(true);
  lists.clear();

  std::vector<SBPrefix> prefix_misses;
  std::vector<SBFullHashResult> empty_full_hash;
  prefix_misses.push_back(Sha256Prefix("http://www.bad.com/malware.html"));
  prefix_misses.push_back(Sha256Prefix("http://www.bad.com/phishing.html"));
  database->CacheHashResults(prefix_misses, empty_full_hash);

  // Prefixes with no full results are misses.
  EXPECT_EQ(database->prefix_miss_cache()->size(), 2U);

  // Update the database.
  PopulateDatabaseForCacheTest(database);

  // Prefix miss cache should be cleared.
  EXPECT_EQ(database->prefix_miss_cache()->size(), 0U);

  // Cache a GetHash miss for a particular prefix, and even though the prefix is
  // in the database, it is flagged as a miss so looking up the associated URL
  // will not succeed.
  prefixes.clear();
  full_hashes.clear();
  prefix_misses.clear();
  empty_full_hash.clear();
  prefix_misses.push_back(Sha256Prefix("www.evil.com/phishing.html"));
  database->CacheHashResults(prefix_misses, empty_full_hash);
  EXPECT_FALSE(database->ContainsUrl(GURL("http://www.evil.com/phishing.html"),
                                     &listname, &prefixes,
                                     &full_hashes, Time::Now()));

  lists.clear();
  prefixes.clear();
  full_hashes.clear();

  // Test receiving a full add chunk.
  host.host = Sha256Prefix("www.fullevil.com/");
  host.entry = SBEntry::Create(SBEntry::ADD_FULL_HASH, 2);
  host.entry->set_chunk_id(20);
  SBFullHash full_add1;
  base::SHA256HashString("www.fullevil.com/bad1.html",
                         &full_add1.full_hash, sizeof(SBFullHash));
  host.entry->SetFullHashAt(0, full_add1);
  SBFullHash full_add2;
  base::SHA256HashString("www.fullevil.com/bad2.html",
                         &full_add2.full_hash, sizeof(SBFullHash));
  host.entry->SetFullHashAt(1, full_add2);

  chunk.chunk_number = 20;
  chunk.is_add = true;
  chunk.hosts.clear();
  chunk.hosts.push_back(host);
  chunks = new std::deque<SBChunk>;
  chunks->push_back(chunk);
  database->UpdateStarted();
  database->GetListsInfo(&lists);
  database->InsertChunks(safe_browsing_util::kMalwareList, chunks);
  database->UpdateFinished(true);

  EXPECT_TRUE(database->ContainsUrl(GURL("http://www.fullevil.com/bad1.html"),
                                    &listname, &prefixes, &full_hashes,
                                    Time::Now()));
  EXPECT_EQ(full_hashes.size(), 1U);
  EXPECT_EQ(0, memcmp(full_hashes[0].hash.full_hash,
                      full_add1.full_hash,
                      sizeof(SBFullHash)));
  lists.clear();
  prefixes.clear();
  full_hashes.clear();

  EXPECT_TRUE(database->ContainsUrl(GURL("http://www.fullevil.com/bad2.html"),
                                    &listname, &prefixes, &full_hashes,
                                    Time::Now()));
  EXPECT_EQ(full_hashes.size(), 1U);
  EXPECT_EQ(0, memcmp(full_hashes[0].hash.full_hash,
                      full_add2.full_hash,
                      sizeof(SBFullHash)));
  lists.clear();
  prefixes.clear();
  full_hashes.clear();

  // Test receiving a full sub chunk, which will remove one of the full adds.
  host.host = Sha256Prefix("www.fullevil.com/");
  host.entry = SBEntry::Create(SBEntry::SUB_FULL_HASH, 1);
  host.entry->set_chunk_id(200);
  host.entry->SetChunkIdAtPrefix(0, 20);
  SBFullHash full_sub;
  base::SHA256HashString("www.fullevil.com/bad1.html",
                         &full_sub.full_hash, sizeof(SBFullHash));
  host.entry->SetFullHashAt(0, full_sub);

  chunk.chunk_number = 200;
  chunk.is_add = false;
  chunk.hosts.clear();
  chunk.hosts.push_back(host);
  chunks = new std::deque<SBChunk>;
  chunks->push_back(chunk);
  database->UpdateStarted();
  database->GetListsInfo(&lists);
  database->InsertChunks(safe_browsing_util::kMalwareList, chunks);
  database->UpdateFinished(true);

  EXPECT_FALSE(database->ContainsUrl(GURL("http://www.fullevil.com/bad1.html"),
                                     &listname, &prefixes, &full_hashes,
                                     Time::Now()));
  EXPECT_EQ(full_hashes.size(), 0U);

  // There should be one remaining full add.
  EXPECT_TRUE(database->ContainsUrl(GURL("http://www.fullevil.com/bad2.html"),
                                    &listname, &prefixes, &full_hashes,
                                    Time::Now()));
  EXPECT_EQ(full_hashes.size(), 1U);
  EXPECT_EQ(0, memcmp(full_hashes[0].hash.full_hash,
                      full_add2.full_hash,
                      sizeof(SBFullHash)));
  lists.clear();
  prefixes.clear();
  full_hashes.clear();

  // Now test an AddDel for the remaining full add.
  database->UpdateStarted();
  database->GetListsInfo(&lists);
  AddDelChunk(database, safe_browsing_util::kMalwareList, 20);
  database->UpdateFinished(true);
  lists.clear();

  EXPECT_FALSE(database->ContainsUrl(GURL("http://www.fullevil.com/bad1.html"),
                                     &listname, &prefixes, &full_hashes,
                                     Time::Now()));
  EXPECT_FALSE(database->ContainsUrl(GURL("http://www.fullevil.com/bad2.html"),
                                     &listname, &prefixes, &full_hashes,
                                     Time::Now()));

  TearDownTestDatabase(database);
}

void PrintStat(const char* name) {
  int value = StatsTable::current()->GetCounterValue(name);
  LOG(INFO) << StringPrintf("%s %d", name, value);
}

std::wstring GetFullSBDataPath(const std::wstring& path) {
  FilePath full_path;
  CHECK(PathService::Get(base::DIR_SOURCE_ROOT, &full_path));
  full_path = full_path.AppendASCII("chrome");
  full_path = full_path.AppendASCII("test");
  full_path = full_path.AppendASCII("data");
  full_path = full_path.AppendASCII("safe_browsing");
  full_path = full_path.Append(FilePath::FromWStringHack(path));
  CHECK(file_util::PathExists(full_path));
  return full_path.ToWStringHack();
}

struct ChunksInfo {
  std::deque<SBChunk>* chunks;
  std::string listname;
};

void PeformUpdate(const std::wstring& initial_db,
                  const std::vector<ChunksInfo>& chunks,
                  std::vector<SBChunkDelete>* deletes) {
// TODO(pinkerton): I don't think posix has any concept of IO counters, but
// we can uncomment this when we implement ProcessMetrics::GetIOCounters
  IoCounters before, after;

  FilePath path;
  PathService::Get(base::DIR_TEMP, &path);
  path = path.AppendASCII("SafeBrowsingTestDatabase");

  // In case it existed from a previous run.
  file_util::Delete(path, false);

  if (!initial_db.empty()) {
    std::wstring full_initial_db = GetFullSBDataPath(initial_db);
    ASSERT_TRUE(file_util::CopyFile(
        FilePath::FromWStringHack(full_initial_db), path));
  }

  SafeBrowsingDatabase* database = SafeBrowsingDatabase::Create();
  database->SetSynchronous();
  EXPECT_TRUE(database->Init(path, NULL));

  Time before_time = Time::Now();
  base::ProcessHandle handle = base::Process::Current().handle();
  scoped_ptr<base::ProcessMetrics> metric(
      base::ProcessMetrics::CreateProcessMetrics(handle));
  CHECK(metric->GetIOCounters(&before));

  std::vector<SBListChunkRanges> lists;
  database->UpdateStarted();
  database->GetListsInfo(&lists);
  database->DeleteChunks(deletes);
  for (size_t i = 0; i < chunks.size(); ++i)
    database->InsertChunks(chunks[i].listname, chunks[i].chunks);

  database->UpdateFinished(true);
  lists.clear();

  CHECK(metric->GetIOCounters(&after));

  LOG(INFO) << StringPrintf("I/O Read Bytes: %d",
      after.ReadTransferCount - before.ReadTransferCount);
  LOG(INFO) << StringPrintf("I/O Write Bytes: %d",
      after.WriteTransferCount - before.WriteTransferCount);
  LOG(INFO) << StringPrintf("I/O Reads: %d",
      after.ReadOperationCount - before.ReadOperationCount);
  LOG(INFO) << StringPrintf("I/O Writes: %d",
      after.WriteOperationCount - before.WriteOperationCount);
  LOG(INFO) << StringPrintf("Finished in %d ms",
      (Time::Now() - before_time).InMilliseconds());

  PrintStat("c:SB.HostSelect");
  PrintStat("c:SB.HostSelectForBloomFilter");
  PrintStat("c:SB.HostReplace");
  PrintStat("c:SB.HostInsert");
  PrintStat("c:SB.HostDelete");
  PrintStat("c:SB.ChunkSelect");
  PrintStat("c:SB.ChunkInsert");
  PrintStat("c:SB.ChunkDelete");
  PrintStat("c:SB.TransactionCommit");

  delete database;
}

void UpdateDatabase(const std::wstring& initial_db,
                    const std::wstring& response_path,
                    const std::wstring& updates_path) {
  // First we read the chunks from disk, so that this isn't counted in IO bytes.
  std::vector<ChunksInfo> chunks;

  SafeBrowsingProtocolParser parser;
  if (!updates_path.empty()) {
    std::wstring data_dir = GetFullSBDataPath(updates_path);
    file_util::FileEnumerator file_enum(
        FilePath::FromWStringHack(data_dir), false,
        file_util::FileEnumerator::FILES);
    while (true) {
      std::wstring file = file_enum.Next().ToWStringHack();
      if (file.empty())
        break;

      int64 size64;
      bool result = file_util::GetFileSize(file, &size64);
      CHECK(result);

      int size = static_cast<int>(size64);
      scoped_array<char> data(new char[size]);
      file_util::ReadFile(file, data.get(), size);

      ChunksInfo info;
      info.chunks = new std::deque<SBChunk>;

      bool re_key;
      result = parser.ParseChunk(data.get(), size, "", "",
                                 &re_key, info.chunks);
      CHECK(result);

      info.listname = WideToASCII(file_util::GetFilenameFromPath(file));
      size_t index = info.listname.find('_');  // Get rid fo the _s or _a.
      info.listname.resize(index);
      info.listname.erase(0, 3);  // Get rid of the 000 etc.

      chunks.push_back(info);
    }
  }

  std::vector<SBChunkDelete>* deletes = new std::vector<SBChunkDelete>;
  if (!response_path.empty()) {
    std::string update;
    std::wstring full_response_path = GetFullSBDataPath(response_path);
    if (file_util::ReadFileToString(full_response_path, &update)) {
      int next_update;
      bool result, rekey, reset;
      std::vector<ChunkUrl> urls;
      result = parser.ParseUpdate(update.c_str(),
                                  static_cast<int>(update.length()),
                                  "",
                                  &next_update,
                                  &rekey,
                                  &reset,
                                  deletes,
                                  &urls);
      DCHECK(result);
      if (!updates_path.empty())
        DCHECK(urls.size() == chunks.size());
    }
  }

  PeformUpdate(initial_db, chunks, deletes);
}

namespace {

std::wstring GetOldSafeBrowsingPath() {
  std::wstring path = L"old";
  file_util::AppendToPath(&path, L"SafeBrowsing");
  return path;
}

std::wstring GetOldResponsePath() {
  std::wstring path = L"old";
  file_util::AppendToPath(&path, L"response");
  return path;
}

std::wstring GetOldUpdatesPath() {
  std::wstring path = L"old";
  file_util::AppendToPath(&path, L"updates");
  return path;
}

}  // namespace

// Counts the IO needed for the initial update of a database.
// test\data\safe_browsing\download_update.py was used to fetch the add/sub
// chunks that are read, in order to get repeatable runs.
TEST(SafeBrowsingDatabase, DISABLED_DatabaseInitialIO) {
  UpdateDatabase(L"", L"", L"initial");
}

// TODO(port): For now on Linux the test below would fail with error below:
// [1004/201323:FATAL:browser/safe_browsing/safe_browsing_database_impl.cc(712)]
// Check failed: false.
//
// Counts the IO needed to update a month old database.
// The data files were generated by running "..\download_update.py postdata"
// in the "safe_browsing\old" directory.
TEST(SafeBrowsingDatabase, DISABLED_DatabaseOldIO) {
  UpdateDatabase(GetOldSafeBrowsingPath(), GetOldResponsePath(),
                 GetOldUpdatesPath());
}

// TODO(port): For now on Linux the test below would fail with error below:
// [1004/201323:FATAL:browser/safe_browsing/safe_browsing_database_impl.cc(712)]
// Check failed: false.
//
// Like DatabaseOldIO but only the deletes.
TEST(SafeBrowsingDatabase, DISABLED_DatabaseOldDeletesIO) {
  UpdateDatabase(GetOldSafeBrowsingPath(), GetOldResponsePath(), L"");
}

// Like DatabaseOldIO but only the updates.
TEST(SafeBrowsingDatabase, DISABLED_DatabaseOldUpdatesIO) {
  UpdateDatabase(GetOldSafeBrowsingPath(), L"", GetOldUpdatesPath());
}

// TODO(port): For now on Linux the test below would fail with error below:
// [1004/201323:FATAL:browser/safe_browsing/safe_browsing_database_impl.cc(712)]
// Check failed: false.
//
// Does a a lot of addel's on very large chunks.
TEST(SafeBrowsingDatabase, DISABLED_DatabaseOldLotsofDeletesIO) {
  std::vector<ChunksInfo> chunks;
  std::vector<SBChunkDelete>* deletes = new std::vector<SBChunkDelete>;
  SBChunkDelete del;
  del.is_sub_del = false;
  del.list_name = safe_browsing_util::kMalwareList;
  del.chunk_del.push_back(ChunkRange(3539, 3579));
  deletes->push_back(del);
  PeformUpdate(GetOldSafeBrowsingPath(), chunks, deletes);
}
