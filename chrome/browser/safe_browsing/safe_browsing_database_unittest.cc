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
#include "googleurl/src/gurl.h"
#include "testing/gtest/include/gtest/gtest.h"

static const wchar_t kSafeBrowsingTestDatabase[] = L"SafeBrowsingTestDatabase";

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

  // Common database test set up code.
  std::wstring GetTestDatabaseName() {
    std::wstring filename;
    PathService::Get(base::DIR_TEMP, &filename);
    filename.push_back(file_util::kPathSeparator);
    filename.append(kSafeBrowsingTestDatabase);
    return filename;
  }

  SafeBrowsingDatabase* SetupTestDatabase() {
    std::wstring filename = GetTestDatabaseName();
    // In case it existed from a previous run.
    file_util::Delete(filename, false);

    SafeBrowsingDatabase* database = SafeBrowsingDatabase::Create();
    database->SetSynchronous();
    EXPECT_TRUE(database->Init(filename, NULL));

    return database;
  }

  void TearDownTestDatabase(SafeBrowsingDatabase* database) {
    file_util::Delete(GetTestDatabaseName(), false);
    delete database;
  }

}  // namespace

// Checks database reading and writing.
TEST(SafeBrowsingDatabase, Database) {
  SafeBrowsingDatabase* database = SetupTestDatabase();

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
  database->InsertChunks("goog-malware", chunks);

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

  database->InsertChunks("goog-malware", chunks);

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
  database->InsertChunks("goog-malware", chunks);
  database->UpdateFinished(true);

  // Make sure they were added correctly.
  std::vector<SBListChunkRanges> lists;
  database->GetListsInfo(&lists);
  EXPECT_EQ(lists.size(), 1U);
  EXPECT_EQ(lists[0].name, "goog-malware");
  EXPECT_EQ(lists[0].adds, "1-3");
  EXPECT_TRUE(lists[0].subs.empty());

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

  database->InsertChunks("goog-malware", chunks);
  database->UpdateFinished(true);

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

  database->GetListsInfo(&lists);
  EXPECT_EQ(lists.size(), 1U);
  EXPECT_EQ(lists[0].name, "goog-malware");
  EXPECT_EQ(lists[0].subs, "4");

  // Test removing all the prefixes from an add chunk.
  AddDelChunk(database, "goog-malware", 2);
  database->UpdateFinished(true);

  EXPECT_FALSE(database->ContainsUrl(GURL("http://www.evil.com/notevil2.html"),
                                     &matching_list, &prefix_hits,
                                     &full_hashes, now));

  EXPECT_FALSE(database->ContainsUrl(GURL("http://www.good.com/good1.html"),
                                     &matching_list, &prefix_hits,
                                     &full_hashes, now));

  EXPECT_FALSE(database->ContainsUrl(GURL("http://www.good.com/good2.html"),
                                     &matching_list, &prefix_hits,
                                     &full_hashes, now));

  database->GetListsInfo(&lists);
  EXPECT_EQ(lists.size(), 1U);
  EXPECT_EQ(lists[0].name, "goog-malware");
  EXPECT_EQ(lists[0].adds, "1,3");
  EXPECT_EQ(lists[0].subs, "4");

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
  database->InsertChunks("goog-malware", chunks);

  // Now remove the dummy entry.  If there are any problems with the
  // transactions, asserts will fire.
  AddDelChunk(database, "goog-malware", 44);

  // Test the subdel command.
  SubDelChunk(database, "goog-malware", 4);
  database->UpdateFinished(true);

  database->GetListsInfo(&lists);
  EXPECT_EQ(lists.size(), 1U);
  EXPECT_EQ(lists[0].name, "goog-malware");
  EXPECT_EQ(lists[0].adds, "1,3");
  EXPECT_EQ(lists[0].subs, "");

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
  database->InsertChunks("goog-malware", chunks);
  database->UpdateFinished(true);

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
  database->InsertChunks("goog-malware", chunks);
  database->UpdateFinished(true);

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
  SafeBrowsingDatabase* database = SetupTestDatabase();

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

  database->InsertChunks("goog-malware", chunks);
  database->UpdateFinished(true);

  // Add an empty ADD and SUB chunk.
  std::vector<SBListChunkRanges> list_chunks_empty;
  database->GetListsInfo(&list_chunks_empty);
  EXPECT_EQ(list_chunks_empty[0].adds, "1,10");

  SBChunk empty_chunk;
  empty_chunk.chunk_number = 19;
  empty_chunk.is_add = true;
  chunks = new std::deque<SBChunk>;
  chunks->push_back(empty_chunk);
  database->InsertChunks("goog-malware", chunks);
  chunks = new std::deque<SBChunk>;
  empty_chunk.chunk_number = 7;
  empty_chunk.is_add = false;
  chunks->push_back(empty_chunk);
  database->InsertChunks("goog-malware", chunks);
  database->UpdateFinished(true);

  list_chunks_empty.clear();
  database->GetListsInfo(&list_chunks_empty);
  EXPECT_EQ(list_chunks_empty[0].adds, "1,10,19");
  EXPECT_EQ(list_chunks_empty[0].subs, "7");

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

  database->InsertChunks("goog-malware", chunks);
  database->UpdateFinished(true);

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

  list_chunks_empty.clear();
  database->GetListsInfo(&list_chunks_empty);
  EXPECT_EQ(list_chunks_empty[0].adds, "1,10,19-22");
  EXPECT_EQ(list_chunks_empty[0].subs, "7");

  // Handle AddDel and SubDel commands for empty chunks.
  AddDelChunk(database, "goog-malware", 21);
  database->UpdateFinished(true);
  list_chunks_empty.clear();
  database->GetListsInfo(&list_chunks_empty);
  EXPECT_EQ(list_chunks_empty[0].adds, "1,10,19-20,22");
  EXPECT_EQ(list_chunks_empty[0].subs, "7");

  SubDelChunk(database, "goog-malware", 7);
  database->UpdateFinished(true);
  list_chunks_empty.clear();
  database->GetListsInfo(&list_chunks_empty);
  EXPECT_EQ(list_chunks_empty[0].adds, "1,10,19-20,22");
  EXPECT_EQ(list_chunks_empty[0].subs, "");

  TearDownTestDatabase(database);
}

void PrintStat(const wchar_t* name) {
#if defined(OS_WIN)
  int value = StatsTable::current()->GetCounterValue(name);
  LOG(INFO) << StringPrintf(L"%ls %d", name, value);
#else
  // TODO(port): Enable when StatsTable is ported.
  NOTIMPLEMENTED();
#endif
}

std::wstring GetFullSBDataPath(const std::wstring& path) {
  std::wstring full_path;
  CHECK(PathService::Get(base::DIR_SOURCE_ROOT, &full_path));
  file_util::AppendToPath(&full_path, L"chrome");
  file_util::AppendToPath(&full_path, L"test");
  file_util::AppendToPath(&full_path, L"data");
  file_util::AppendToPath(&full_path, L"safe_browsing");
  file_util::AppendToPath(&full_path, path);
  CHECK(file_util::PathExists(full_path));
  return full_path;
}

struct ChunksInfo {
  std::deque<SBChunk>* chunks;
  std::string listname;
};

void PeformUpdate(const std::wstring& initial_db,
                  const std::vector<ChunksInfo>& chunks,
                  std::vector<SBChunkDelete>* deletes) {
  IoCounters before, after;

  std::wstring filename;
  PathService::Get(base::DIR_TEMP, &filename);
  filename.push_back(file_util::kPathSeparator);
  filename.append(L"SafeBrowsingTestDatabase");
  // In case it existed from a previous run.
  file_util::Delete(filename, false);

  if (!initial_db.empty()) {
    std::wstring full_initial_db = GetFullSBDataPath(initial_db);
    ASSERT_TRUE(file_util::CopyFile(full_initial_db, filename));
  }

  SafeBrowsingDatabase* database = SafeBrowsingDatabase::Create();
  database->SetSynchronous();
  EXPECT_TRUE(database->Init(filename, NULL));

  Time before_time = Time::Now();
  ProcessHandle handle = Process::Current().handle();
  scoped_ptr<process_util::ProcessMetrics> metric(
      process_util::ProcessMetrics::CreateProcessMetrics(handle));
  CHECK(metric->GetIOCounters(&before));

  database->DeleteChunks(deletes);

  for (size_t i = 0; i < chunks.size(); ++i)
    database->InsertChunks(chunks[i].listname, chunks[i].chunks);

  database->UpdateFinished(true);

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

  PrintStat(L"c:SB.HostSelect");
  PrintStat(L"c:SB.HostSelectForBloomFilter");
  PrintStat(L"c:SB.HostReplace");
  PrintStat(L"c:SB.HostInsert");
  PrintStat(L"c:SB.HostDelete");
  PrintStat(L"c:SB.ChunkSelect");
  PrintStat(L"c:SB.ChunkInsert");
  PrintStat(L"c:SB.ChunkDelete");
  PrintStat(L"c:SB.TransactionCommit");

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
        data_dir, false, file_util::FileEnumerator::FILES);
    while (true) {
      std::wstring file = file_enum.Next();
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

const wchar_t* GetOldSafeBrowsingPath() {
  std::wstring path = L"old";
  file_util::AppendToPath(&path, L"SafeBrowsing");
  return path.c_str();
}

const wchar_t* GetOldResponsePath() {
  std::wstring path = L"old";
  file_util::AppendToPath(&path, L"response");
  return path.c_str();
}

const wchar_t* GetOldUpdatesPath() {
  std::wstring path = L"old";
  file_util::AppendToPath(&path, L"updates");
  return path.c_str();
}

}  // namespace

// Counts the IO needed for the initial update of a database.
// test\data\safe_browsing\download_update.py was used to fetch the add/sub
// chunks that are read, in order to get repeatable runs.
TEST(SafeBrowsingDatabase, DatabaseInitialIO) {
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
  del.list_name = "goog-malware-shavar";
  del.chunk_del.push_back(ChunkRange(3539, 3579));
  deletes->push_back(del);
  PeformUpdate(GetOldSafeBrowsingPath(), chunks, deletes);
}
