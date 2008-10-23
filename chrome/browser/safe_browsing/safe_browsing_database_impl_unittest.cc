// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Unit tests for the SafeBrowsing storage system (specific to the
// SafeBrowsingDatabaseImpl implementation).

#include "base/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/process_util.h"
#include "base/sha2.h"
#include "base/stats_counters.h"
#include "base/string_util.h"
#include "base/time.h"
#include "chrome/browser/safe_browsing/protocol_parser.h"
#include "chrome/browser/safe_browsing/safe_browsing_database_impl.h"
#include "googleurl/src/gurl.h"
#include "testing/gtest/include/gtest/gtest.h"

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

  std::vector<SBPrefix> prefixes;
  database->CacheHashResults(prefixes, results);
}

TEST(SafeBrowsingDatabaseImpl, HashCaching) {
  std::wstring filename;
  PathService::Get(base::DIR_TEMP, &filename);
  filename.push_back(file_util::kPathSeparator);
  filename.append(L"SafeBrowsingTestDatabase");
  file_util::Delete(filename, false);  // In case it existed from a previous run.

  SafeBrowsingDatabaseImpl database;
  database.SetSynchronous();
  EXPECT_TRUE(database.Init(filename, NULL));

  PopulateDatabaseForCacheTest(&database);

  // We should have both full hashes in the cache.
  EXPECT_EQ(database.hash_cache_.size(), 2U);

  // Test the cache lookup for the first prefix.
  std::string list;
  std::vector<SBPrefix> prefixes;
  std::vector<SBFullHashResult> full_hashes;
  database.ContainsUrl(GURL("http://www.evil.com/phishing.html"),
                       &list, &prefixes, &full_hashes, Time::Now());
  EXPECT_EQ(full_hashes.size(), 1U);

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
  host.entry = SBEntry::Create(SBEntry::SUB_PREFIX, 2);
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
  database.InsertChunks("goog-malware-shavar", chunks);

  // This prefix should still be there.
  database.ContainsUrl(GURL("http://www.evil.com/malware.html"),
                       &list, &prefixes, &full_hashes, Time::Now());
  EXPECT_EQ(full_hashes.size(), 1U);
  base::SHA256HashString("www.evil.com/malware.html",
                         &full_hash.hash, sizeof(SBFullHash));
  EXPECT_EQ(memcmp(&full_hashes[0].hash,
                   &full_hash.hash, sizeof(SBFullHash)), 0);

  prefixes.clear();
  full_hashes.clear();

  // This prefix should be gone.
  database.ContainsUrl(GURL("http://www.evil.com/phishing.html"),
                       &list, &prefixes, &full_hashes, Time::Now());
  EXPECT_EQ(full_hashes.size(), 0U);

  prefixes.clear();
  full_hashes.clear();

  // Test that an AddDel for the original chunk removes the last cached entry.
  AddDelChunk(&database, "goog-malware-shavar", 1);
  database.ContainsUrl(GURL("http://www.evil.com/malware.html"),
                       &list, &prefixes, &full_hashes, Time::Now());
  EXPECT_EQ(full_hashes.size(), 0U);
  EXPECT_EQ(database.hash_cache_.size(), 0U);

  prefixes.clear();
  full_hashes.clear();

  // Test that the cache won't return expired values. First we have to adjust
  // the cached entries' received time to make them older, since the database
  // cache insert uses Time::Now(). First, store some entries.
  PopulateDatabaseForCacheTest(&database);
  EXPECT_EQ(database.hash_cache_.size(), 2U);

  // Now adjust one of the entries times to be in the past.
  Time expired = Time::Now() - TimeDelta::FromMinutes(60);
  SBPrefix key;
  memcpy(&key, &full_hash.hash, sizeof(SBPrefix));
  SafeBrowsingDatabaseImpl::HashList& entries = database.hash_cache_[key];
  SafeBrowsingDatabaseImpl::HashCacheEntry entry = entries.front();
  entries.pop_front();
  entry.received = expired;
  entries.push_back(entry);

  database.ContainsUrl(GURL("http://www.evil.com/malware.html"),
                       &list, &prefixes, &full_hashes, expired);
  EXPECT_EQ(full_hashes.size(), 0U);

  // Expired entry was dumped.
  EXPECT_EQ(database.hash_cache_.size(), 1U);

  // This entry should still exist.
  database.ContainsUrl(GURL("http://www.evil.com/phishing.html"),
                       &list, &prefixes, &full_hashes, expired);
  EXPECT_EQ(full_hashes.size(), 1U);


  // Testing prefix miss caching. First, we clear out the existing database,
  // Since PopulateDatabaseForCacheTest() doesn't handle adding duplicate
  // chunks.
  AddDelChunk(&database, "goog-malware-shavar", 1);

  std::vector<SBPrefix> prefix_misses;
  std::vector<SBFullHashResult> empty_full_hash;
  prefix_misses.push_back(Sha256Prefix("http://www.bad.com/malware.html"));
  prefix_misses.push_back(Sha256Prefix("http://www.bad.com/phishing.html"));
  database.CacheHashResults(prefix_misses, empty_full_hash);

  // Prefixes with no full results are misses.
  EXPECT_EQ(database.prefix_miss_cache_.size(), 2U);

  // Update the database.
  PopulateDatabaseForCacheTest(&database);

  // Prefix miss cache should be cleared.
  EXPECT_EQ(database.prefix_miss_cache_.size(), 0U);
}
