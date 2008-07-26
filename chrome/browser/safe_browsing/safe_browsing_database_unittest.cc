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
//
// Unit tests for the SafeBrowsing storage system (SafeBrowsingDatabase).

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
#include "testing/gtest/include/gtest/gtest.h"

namespace {
  SBPrefix Sha256Prefix(const std::string& str) {
    SBPrefix hash;
    base::SHA256HashString(str, &hash, sizeof(hash));
    return hash;
  }
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

void AddDelChunk(SafeBrowsingDatabase* db, const std::string& list, int chunk_id) {
  DelChunk(db, list, chunk_id, false);
}

void SubDelChunk(SafeBrowsingDatabase* db, const std::string& list, int chunk_id) {
  DelChunk(db, list, chunk_id, true);
}

// Checks database reading/writing.
TEST(SafeBrowsing, Database) {
  std::wstring filename;
  PathService::Get(base::DIR_TEMP, &filename);
  filename.push_back(file_util::kPathSeparator);
  filename.append(L"SafeBrowsingTestDatabase");
  DeleteFile(filename.c_str());  // In case it existed from a previous run.

  SafeBrowsingDatabase database;
  database.set_synchronous();
  EXPECT_TRUE(database.Init(filename, NULL));

  // Add a simple chunk with one hostkey.
  SBChunkHost host;
  host.host = Sha256Prefix("www.evil.com/");
  host.entry = SBEntry::Create(SBEntry::ADD_PREFIX, 2);
  host.entry->set_chunk_id(1);
  host.entry->SetPrefixAt(0, Sha256Prefix("www.evil.com/phishing.html"));
  host.entry->SetPrefixAt(1, Sha256Prefix("www.evil.com/malware.html"));

  SBChunk chunk;
  chunk.chunk_number = 1;
  chunk.hosts.push_back(host);

  std::deque<SBChunk>* chunks = new std::deque<SBChunk>;
  chunks->push_back(chunk);
  database.InsertChunks("goog-malware", chunks);

  // Add another chunk with two different hostkeys.
  host.host = Sha256Prefix("www.evil.com/");
  host.entry = SBEntry::Create(SBEntry::ADD_PREFIX, 2);
  host.entry->set_chunk_id(1);
  host.entry->SetPrefixAt(0, Sha256Prefix("www.evil.com/notevil1.html"));
  host.entry->SetPrefixAt(1, Sha256Prefix("www.evil.com/notevil2.html"));

  chunk.chunk_number = 2;
  chunk.hosts.clear();
  chunk.hosts.push_back(host);

  host.host = Sha256Prefix("www.good.com/");
  host.entry = SBEntry::Create(SBEntry::ADD_PREFIX, 2);
  host.entry->SetPrefixAt(0, Sha256Prefix("www.good.com/good1.html"));
  host.entry->SetPrefixAt(1, Sha256Prefix("www.good.com/good2.html"));

  chunk.hosts.push_back(host);

  chunks = new std::deque<SBChunk>;
  chunks->push_back(chunk);

  database.InsertChunks("goog-malware", chunks);

  // and a chunk with an IP-based host
  host.host = Sha256Prefix("192.168.0.1/");
  host.entry = SBEntry::Create(SBEntry::ADD_PREFIX, 1);
  host.entry->SetPrefixAt(0, Sha256Prefix("192.168.0.1/malware.html"));

  chunk.chunk_number = 3;
  chunk.hosts.clear();
  chunk.hosts.push_back(host);

  chunks = new std::deque<SBChunk>;
  chunks->push_back(chunk);
  database.InsertChunks("goog-malware", chunks);


  // Make sure they were added correctly.
  std::vector<SBListChunkRanges> lists;
  database.GetListsInfo(&lists);
  EXPECT_EQ(lists.size(), 1);
  EXPECT_EQ(lists[0].name, "goog-malware");
  EXPECT_EQ(lists[0].adds, "1-3");
  EXPECT_TRUE(lists[0].subs.empty());

  const Time now = Time::Now();
  std::vector<SBFullHashResult> full_hashes;
  std::vector<SBPrefix> prefix_hits;
  std::string matching_list;
  EXPECT_TRUE(database.ContainsUrl(GURL("http://www.evil.com/phishing.html"),
                                   &matching_list, &prefix_hits,
                                   &full_hashes, now));
  EXPECT_EQ(prefix_hits[0], Sha256Prefix("www.evil.com/phishing.html"));
  EXPECT_EQ(prefix_hits.size(), 1);

  EXPECT_TRUE(database.ContainsUrl(GURL("http://www.evil.com/malware.html"),
                                   &matching_list, &prefix_hits,
                                   &full_hashes, now));

  EXPECT_TRUE(database.ContainsUrl(GURL("http://www.evil.com/notevil1.html"),
                                   &matching_list, &prefix_hits,
                                   &full_hashes, now));

  EXPECT_TRUE(database.ContainsUrl(GURL("http://www.evil.com/notevil2.html"),
                                   &matching_list, &prefix_hits,
                                   &full_hashes, now));

  EXPECT_TRUE(database.ContainsUrl(GURL("http://www.good.com/good1.html"),
                                   &matching_list, &prefix_hits,
                                   &full_hashes, now));

  EXPECT_TRUE(database.ContainsUrl(GURL("http://www.good.com/good2.html"),
                                   &matching_list, &prefix_hits,
                                   &full_hashes, now));

  EXPECT_TRUE(database.ContainsUrl(GURL("http://192.168.0.1/malware.html"),
                                   &matching_list, &prefix_hits,
                                   &full_hashes, now));

  EXPECT_FALSE(database.ContainsUrl(GURL("http://www.evil.com/"),
                                    &matching_list, &prefix_hits,
                                    &full_hashes, now));
  EXPECT_EQ(prefix_hits.size(), 0);

  EXPECT_FALSE(database.ContainsUrl(GURL("http://www.evil.com/robots.txt"),
                                    &matching_list, &prefix_hits,
                                    &full_hashes, now));

  // Test removing a single prefix from the add chunk.
  host.host = Sha256Prefix("www.evil.com/");
  host.entry = SBEntry::Create(SBEntry::SUB_PREFIX, 2);
  host.entry->set_chunk_id(2);
  host.entry->SetChunkIdAtPrefix(0, 2);
  host.entry->SetPrefixAt(0, Sha256Prefix("www.evil.com/notevil1.html"));

  chunk.chunk_number = 4;
  chunk.hosts.clear();
  chunk.hosts.push_back(host);

  chunks = new std::deque<SBChunk>;
  chunks->push_back(chunk);

  database.InsertChunks("goog-malware", chunks);

  EXPECT_TRUE(database.ContainsUrl(GURL("http://www.evil.com/phishing.html"),
                                   &matching_list, &prefix_hits,
                                   &full_hashes, now));
  EXPECT_EQ(prefix_hits[0], Sha256Prefix("www.evil.com/phishing.html"));
  EXPECT_EQ(prefix_hits.size(), 1);

  EXPECT_FALSE(database.ContainsUrl(GURL("http://www.evil.com/notevil1.html"),
                                    &matching_list, &prefix_hits,
                                    &full_hashes, now));
  EXPECT_EQ(prefix_hits.size(), 0);

  EXPECT_TRUE(database.ContainsUrl(GURL("http://www.evil.com/notevil2.html"),
                                   &matching_list, &prefix_hits,
                                   &full_hashes, now));

  EXPECT_TRUE(database.ContainsUrl(GURL("http://www.good.com/good1.html"),
                                   &matching_list, &prefix_hits,
                                   &full_hashes, now));

  EXPECT_TRUE(database.ContainsUrl(GURL("http://www.good.com/good2.html"),
                                   &matching_list, &prefix_hits,
                                   &full_hashes, now));

  database.GetListsInfo(&lists);
  EXPECT_EQ(lists.size(), 1);
  EXPECT_EQ(lists[0].name, "goog-malware");
  EXPECT_EQ(lists[0].subs, "4");

  // Test removing all the prefixes from an add chunk.
  AddDelChunk(&database, "goog-malware", 2);
  EXPECT_FALSE(database.ContainsUrl(GURL("http://www.evil.com/notevil2.html"),
                                    &matching_list, &prefix_hits,
                                    &full_hashes, now));

  EXPECT_FALSE(database.ContainsUrl(GURL("http://www.good.com/good1.html"),
                                    &matching_list, &prefix_hits,
                                    &full_hashes, now));

  EXPECT_FALSE(database.ContainsUrl(GURL("http://www.good.com/good2.html"),
                                    &matching_list, &prefix_hits,
                                    &full_hashes, now));

  database.GetListsInfo(&lists);
  EXPECT_EQ(lists.size(), 1);
  EXPECT_EQ(lists[0].name, "goog-malware");
  EXPECT_EQ(lists[0].subs, "4");

  // The adddel command exposed a bug in the transaction code where any
  // transaction after it would fail.  Add a dummy entry and remove it to
  // make sure the transcation work fine.
  host.host = Sha256Prefix("www.redherring.com/");
  host.entry = SBEntry::Create(SBEntry::ADD_PREFIX, 1);
  host.entry->set_chunk_id(1);
  host.entry->SetPrefixAt(0, Sha256Prefix("www.redherring.com/index.html"));

  chunk.chunk_number = 44;
  chunk.hosts.clear();
  chunk.hosts.push_back(host);

  chunks = new std::deque<SBChunk>;
  chunks->push_back(chunk);
  database.InsertChunks("goog-malware", chunks);

  // Now remove the dummy entry.  If there are any problems with the
  // transactions, asserts will fire.
  AddDelChunk(&database, "goog-malware", 44);

  // Test the subdel command.
  SubDelChunk(&database, "goog-malware", 4);
  database.GetListsInfo(&lists);
  EXPECT_EQ(lists.size(), 1);
  EXPECT_EQ(lists[0].name, "goog-malware");
  EXPECT_EQ(lists[0].subs, "");

  // Test a sub command coming in before the add.
  host.host = Sha256Prefix("www.notevilanymore.com/");
  host.entry = SBEntry::Create(SBEntry::SUB_PREFIX, 0);
  host.entry->set_chunk_id(10);

  chunk.chunk_number = 5;
  chunk.hosts.clear();
  chunk.hosts.push_back(host);

  chunks = new std::deque<SBChunk>;
  chunks->push_back(chunk);
  database.InsertChunks("goog-malware", chunks);

  EXPECT_FALSE(database.ContainsUrl(GURL("http://www.notevilanymore.com/index.html"),
                                    &matching_list, &prefix_hits,
                                    &full_hashes, now));

  // Now insert the tardy add chunk.
  host.host = Sha256Prefix("www.notevilanymore.com/");
  host.entry = SBEntry::Create(SBEntry::ADD_PREFIX, 2);
  host.entry->SetPrefixAt(0, Sha256Prefix("www.notevilanymore.com/index.html"));
  host.entry->SetPrefixAt(1, Sha256Prefix("www.notevilanymore.com/good.html"));

  chunk.chunk_number = 10;
  chunk.hosts.clear();
  chunk.hosts.push_back(host);

  chunks = new std::deque<SBChunk>;
  chunks->push_back(chunk);
  database.InsertChunks("goog-malware", chunks);

  EXPECT_FALSE(database.ContainsUrl(GURL("http://www.notevilanymore.com/index.html"),
                                    &matching_list, &prefix_hits,
                                    &full_hashes, now));

  EXPECT_FALSE(database.ContainsUrl(GURL("http://www.notevilanymore.com/good.html"),
                                    &matching_list, &prefix_hits,
                                    &full_hashes, now));

  DeleteFile(filename.c_str());  // Clean up.
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
  chunk.hosts.push_back(host);

  std::deque<SBChunk>* chunks = new std::deque<SBChunk>;
  chunks->push_back(chunk);
  database->InsertChunks("goog-malware-shavar", chunks);

  // Add the GetHash results to the cache.
  SBFullHashResult full_hash;
  base::SHA256HashString("www.evil.com/phishing.html",
                         &full_hash.hash, sizeof(SBFullHash));
  full_hash.list_name = "goog-malware-shavar";
  full_hash.add_chunk_id = 1;

  std::vector<SBFullHashResult> results;
  results.push_back(full_hash);

  base::SHA256HashString("www.evil.com/malware.html",
                         &full_hash.hash, sizeof(SBFullHash));
  results.push_back(full_hash);

  database->CacheHashResults(results);
}

TEST(SafeBrowsing, HashCaching) {
  std::wstring filename;
  PathService::Get(base::DIR_TEMP, &filename);
  filename.push_back(file_util::kPathSeparator);
  filename.append(L"SafeBrowsingTestDatabase");
  DeleteFile(filename.c_str());  // In case it existed from a previous run.

  SafeBrowsingDatabase database;
  database.set_synchronous();
  EXPECT_TRUE(database.Init(filename, NULL));

  PopulateDatabaseForCacheTest(&database);

  // We should have both full hashes in the cache.
  EXPECT_EQ(database.hash_cache_.size(), 2);

  // Test the cache lookup for the first prefix.
  std::string list;
  std::vector<SBPrefix> prefixes;
  std::vector<SBFullHashResult> full_hashes;
  database.ContainsUrl(GURL("http://www.evil.com/phishing.html"),
                       &list, &prefixes, &full_hashes, Time::Now());
  EXPECT_EQ(full_hashes.size(), 1);

  SBFullHashResult full_hash;
  base::SHA256HashString("www.evil.com/phishing.html",
                         &full_hash.hash, sizeof(SBFullHash));
  EXPECT_EQ(memcmp(&full_hashes[0].hash,
                   &full_hash.hash, sizeof(SBFullHash)), 0);

  prefixes.clear();
  full_hashes.clear();

  // Test the cache lookup for the second prefix.
  database.ContainsUrl(GURL("http://www.evil.com/malware.html"),
                       &list, &prefixes, &full_hashes, Time::Now());
  EXPECT_EQ(full_hashes.size(), 1);
  base::SHA256HashString("www.evil.com/malware.html",
                         &full_hash.hash, sizeof(SBFullHash));
  EXPECT_EQ(memcmp(&full_hashes[0].hash,
                   &full_hash.hash, sizeof(SBFullHash)), 0);

  prefixes.clear();
  full_hashes.clear();

  // Test removing a prefix via a sub chunk.
  SBChunkHost host;
  host.host = Sha256Prefix("www.evil.com/");
  host.entry = SBEntry::Create(SBEntry::SUB_PREFIX, 2);
  host.entry->set_chunk_id(1);
  host.entry->SetChunkIdAtPrefix(0, 1);
  host.entry->SetPrefixAt(0, Sha256Prefix("www.evil.com/phishing.html"));

  SBChunk chunk;
  chunk.chunk_number = 2;
  chunk.hosts.clear();
  chunk.hosts.push_back(host);
  std::deque<SBChunk>* chunks = new std::deque<SBChunk>;
  chunks->push_back(chunk);
  database.InsertChunks("goog-malware-shavar", chunks);

  // This prefix should still be there.
  database.ContainsUrl(GURL("http://www.evil.com/malware.html"),
                       &list, &prefixes, &full_hashes, Time::Now());
  EXPECT_EQ(full_hashes.size(), 1);
  base::SHA256HashString("www.evil.com/malware.html",
                         &full_hash.hash, sizeof(SBFullHash));
  EXPECT_EQ(memcmp(&full_hashes[0].hash,
                   &full_hash.hash, sizeof(SBFullHash)), 0);

  prefixes.clear();
  full_hashes.clear();

  // This prefix should be gone.
  database.ContainsUrl(GURL("http://www.evil.com/phishing.html"),
                       &list, &prefixes, &full_hashes, Time::Now());
  EXPECT_EQ(full_hashes.size(), 0);

  prefixes.clear();
  full_hashes.clear();

  // Test that an AddDel for the original chunk removes the last cached entry.
  AddDelChunk(&database, "goog-malware-shavar", 1);
  database.ContainsUrl(GURL("http://www.evil.com/malware.html"),
                       &list, &prefixes, &full_hashes, Time::Now());
  EXPECT_EQ(full_hashes.size(), 0);
  EXPECT_EQ(database.hash_cache_.size(), 0);

  prefixes.clear();
  full_hashes.clear();

  // Test that the cache won't return expired values. First we have to adjust
  // the cached entries' received time to make them older, since the database
  // cache insert uses Time::Now(). First, store some entries.
  PopulateDatabaseForCacheTest(&database);
  EXPECT_EQ(database.hash_cache_.size(), 2);

  // Now adjust one of the entries times to be in the past.
  Time expired = Time::Now() - TimeDelta::FromMinutes(60);
  SBPrefix key;
  memcpy(&key, &full_hash.hash, sizeof(SBPrefix));
  SafeBrowsingDatabase::HashList& entries = database.hash_cache_[key];
  SafeBrowsingDatabase::HashCacheEntry entry = entries.front();
  entries.pop_front();
  entry.received = expired;
  entries.push_back(entry);

  database.ContainsUrl(GURL("http://www.evil.com/malware.html"),
                       &list, &prefixes, &full_hashes, expired);
  EXPECT_EQ(full_hashes.size(), 0);

  // Expired entry was dumped.
  EXPECT_EQ(database.hash_cache_.size(), 1);

  // This entry should still exist.
  database.ContainsUrl(GURL("http://www.evil.com/phishing.html"),
                       &list, &prefixes, &full_hashes, expired);
  EXPECT_EQ(full_hashes.size(), 1);
}

void PrintStat(const wchar_t* name) {
  int value = StatsTable::current()->GetCounterValue(name);
  std::wstring out = StringPrintf(L"%s %d\r\n", name, value);
  OutputDebugStringW(out.c_str());
}

std::wstring GetFullSBDataPath(const std::wstring& path) {
  std::wstring full_path;
  CHECK(PathService::Get(base::DIR_SOURCE_ROOT, &full_path));
  file_util::AppendToPath(&full_path, L"chrome\\test\\data\\safe_browsing");
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
  DeleteFile(filename.c_str());  // In case it existed from a previous run.

  if (!initial_db.empty()) {
    std::wstring full_initial_db = GetFullSBDataPath(initial_db);
    ASSERT_TRUE(file_util::CopyFile(full_initial_db, filename));
  }

  SafeBrowsingDatabase database;
  database.set_synchronous();
  EXPECT_TRUE(database.Init(filename, NULL));

  Time before_time = Time::Now();
  ProcessHandle handle = Process::Current().handle();
  scoped_ptr<process_util::ProcessMetrics> metric(
      process_util::ProcessMetrics::CreateProcessMetrics(handle));
  CHECK(metric->GetIOCounters(&before));

  database.DeleteChunks(deletes);

  for (size_t i = 0; i < chunks.size(); ++i)
    database.InsertChunks(chunks[i].listname, chunks[i].chunks);

  CHECK(metric->GetIOCounters(&after));

  OutputDebugStringA(StringPrintf("I/O Read Bytes: %d\r\n",
      after.ReadTransferCount - before.ReadTransferCount).c_str());
  OutputDebugStringA(StringPrintf("I/O Write Bytes: %d\r\n",
      after.WriteTransferCount - before.WriteTransferCount).c_str());
  OutputDebugStringA(StringPrintf("I/O Reads: %d\r\n",
      after.ReadOperationCount - before.ReadOperationCount).c_str());
  OutputDebugStringA(StringPrintf("I/O Writes: %d\r\n",
      after.WriteOperationCount - before.WriteOperationCount).c_str());
  OutputDebugStringA(StringPrintf("Finished in %d ms\r\n",
    (Time::Now() - before_time).InMilliseconds()).c_str());

  PrintStat(L"c:SB.HostSelect");
  PrintStat(L"c:SB.HostSelectForBloomFilter");
  PrintStat(L"c:SB.HostReplace");
  PrintStat(L"c:SB.HostInsert");
  PrintStat(L"c:SB.HostDelete");
  PrintStat(L"c:SB.ChunkSelect");
  PrintStat(L"c:SB.ChunkInsert");
  PrintStat(L"c:SB.ChunkDelete");
  PrintStat(L"c:SB.TransactionCommit");
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
      result = parser.ParseChunk(data.get(), size, "", "", &re_key, info.chunks);
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

// Counts the IO needed for the initial update of a database.
// test\data\safe_browsing\download_update.py was used to fetch the add/sub
// chunks that are read, in order to get repeatable runs.
TEST(SafeBrowsing, DISABLED_DatabaseInitialIO) {
  UpdateDatabase(L"", L"", L"initial");
}

// Counts the IO needed to update a month old database.
// The data files were generated by running "..\download_update.py postdata"
// in the "safe_browsing\old" directory.
TEST(SafeBrowsing, DISABLED_DatabaseOldIO) {
  UpdateDatabase(L"old\\SafeBrowsing", L"old\\response", L"old\\updates");
}

// Like DatabaseOldIO but only the deletes.
TEST(SafeBrowsing, DISABLED_DatabaseOldDeletesIO) {
  UpdateDatabase(L"old\\SafeBrowsing", L"old\\response", L"");
}

// Like DatabaseOldIO but only the updates.
TEST(SafeBrowsing, DISABLED_DatabaseOldUpdatesIO) {
  UpdateDatabase(L"old\\SafeBrowsing", L"", L"old\\updates");
}

// Does a a lot of addel's on very large chunks.
TEST(SafeBrowsing, DISABLED_DatabaseOldLotsofDeletesIO) {
  std::vector<ChunksInfo> chunks;
  std::vector<SBChunkDelete>* deletes = new std::vector<SBChunkDelete>;
  SBChunkDelete del;
  del.is_sub_del = false;
  del.list_name = "goog-malware-shavar";
  del.chunk_del.push_back(ChunkRange(3539, 3579));
  deletes->push_back(del);
  PeformUpdate(L"old\\SafeBrowsing", chunks, deletes);
}