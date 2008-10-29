// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/safe_browsing_database_bloom.h"

#include "base/compiler_specific.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/platform_thread.h"
#include "base/sha2.h"
#include "base/string_util.h"
#include "chrome/browser/safe_browsing/bloom_filter.h"
#include "chrome/browser/safe_browsing/chunk_range.h"
#include "chrome/common/sqlite_compiled_statement.h"
#include "chrome/common/sqlite_utils.h"
#include "googleurl/src/gurl.h"

using base::Time;
using base::TimeDelta;

// Database version.  If this is different than what's stored on disk, the
// database is reset.
static const int kDatabaseVersion = 6;

// Don't want to create too small of a bloom filter initially while we're
// downloading the data and then keep having to rebuild it.
static const int kBloomFilterMinSize = 250000;

// How many bits to use per item.  See the design doc for more information.
static const int kBloomFilterSizeRatio = 13;

// When we awake from a low power state, we try to avoid doing expensive disk
// operations for a few minutes to let the system page itself in and settle
// down.
static const int kOnResumeHoldupMs = 5 * 60 * 1000;  // 5 minutes.

// The maximum staleness for a cached entry.
static const int kMaxStalenessMinutes = 45;

// Implementation --------------------------------------------------------------

SafeBrowsingDatabaseBloom::SafeBrowsingDatabaseBloom()
    : db_(NULL),
      init_(false),
      ALLOW_THIS_IN_INITIALIZER_LIST(reset_factory_(this)),
      ALLOW_THIS_IN_INITIALIZER_LIST(resume_factory_(this)),
      did_resume_(false) {
}

SafeBrowsingDatabaseBloom::~SafeBrowsingDatabaseBloom() {
  Close();
}

bool SafeBrowsingDatabaseBloom::Init(const std::wstring& filename,
                                     Callback0::Type* chunk_inserted_callback) {
  DCHECK(!init_ && filename_.empty());

  filename_ = filename + L" Bloom";
  if (!Open())
    return false;

  bool load_filter = false;
  if (!DoesSqliteTableExist(db_, "add_prefix")) {
    if (!CreateTables()) {
      // Database could be corrupt, try starting from scratch.
      if (!ResetDatabase())
        return false;
    }
  } else if (!CheckCompatibleVersion()) {
    if (!ResetDatabase())
      return false;
  } else {
    load_filter = true;
  }

  add_count_ = GetAddPrefixCount();
  bloom_filter_filename_ = BloomFilterFilename(filename_);

  if (load_filter) {
    LoadBloomFilter();
  } else {
    bloom_filter_ =
        new BloomFilter(kBloomFilterMinSize * kBloomFilterSizeRatio);
  }

  init_ = true;
  chunk_inserted_callback_.reset(chunk_inserted_callback);

  return true;
}

bool SafeBrowsingDatabaseBloom::Open() {
  if (sqlite3_open(WideToUTF8(filename_).c_str(), &db_) != SQLITE_OK)
    return false;

  // Run the database in exclusive mode. Nobody else should be accessing the
  // database while we're running, and this will give somewhat improved perf.
  sqlite3_exec(db_, "PRAGMA locking_mode=EXCLUSIVE", NULL, NULL, NULL);

  statement_cache_.reset(new SqliteStatementCache(db_));

  return true;
}

bool SafeBrowsingDatabaseBloom::Close() {
  if (!db_)
    return true;

  statement_cache_.reset();  // Must free statements before closing DB.
  bool result = sqlite3_close(db_) == SQLITE_OK;
  db_ = NULL;

  return result;
}

bool SafeBrowsingDatabaseBloom::CreateTables() {
  SQLTransaction transaction(db_);
  transaction.Begin();

  // Store 32 bit add prefixes here.
  if (sqlite3_exec(db_, "CREATE TABLE add_prefix ("
      "chunk INTEGER,"
      "prefix INTEGER)",
      NULL, NULL, NULL) != SQLITE_OK) {
    return false;
  }

  // Store 32 sub prefixes here.
  if (sqlite3_exec(db_, "CREATE TABLE sub_prefix ("
                   "chunk INTEGER,"
                   "add_chunk INTEGER,"
                   "prefix INTEGER)",
                   NULL, NULL, NULL) != SQLITE_OK) {
    return false;
  }

  // TODO(paulg): Store 256 bit add full prefixes and GetHash results here.
  // 'receive_time' is used for testing the staleness of GetHash results.
  // if (sqlite3_exec(db_, "CREATE TABLE full_add_prefix ("
  //                  "chunk INTEGER,"
  //                  "prefix INTEGER,"
  //                  "receive_time INTEGER,"
  //                  "full_prefix BLOB)",
  //                  NULL, NULL, NULL) != SQLITE_OK) {
  //   return false;
  // }

  // TODO(paulg): These tables are going to contain very few entries, so we
  // need to measure if it's worth keeping an index on them.
  // sqlite3_exec(db_,
  //              "CREATE INDEX full_add_prefix_chunk ON full_prefix(chunk)",
  //              NULL, NULL, NULL);
  // sqlite3_exec(db_,
  //              "CREATE INDEX full_add_prefix_prefix ON full_prefix(prefix)",
  //             NULL, NULL, NULL);

  // TODO(paulg): Store 256 bit sub full prefixes here.
  // if (sqlite3_exec(db_, "CREATE TABLE full_sub_prefix ("
  //                  "chunk INTEGER,"
  //                  "add_chunk INTEGER,"
  //                  "prefix INTEGER,"
  //                  "receive_time INTEGER,"
  //                  "full_prefix BLOB)",
  //                  NULL, NULL, NULL) != SQLITE_OK) {
  //   return false;
  // }

  // TODO(paulg): These tables are going to contain very few entries, so we
  // need to measure if it's worth keeping an index on them.
  // sqlite3_exec(
  //     db_,
  //     "CREATE INDEX full_sub_prefix_chunk ON full_sub_prefix(chunk)",
  //     NULL, NULL, NULL);
  // sqlite3_exec(
  //     db_, 
  //     "CREATE INDEX full_sub_prefix_add_chunk ON full_sub_prefix(add_chunk)",
  //     NULL, NULL, NULL);
  // sqlite3_exec(
  //     db_,
  //     "CREATE INDEX full_sub_prefix_prefix ON full_sub_prefix(prefix)",
  //     NULL, NULL, NULL);

  if (sqlite3_exec(db_, "CREATE TABLE list_names ("
                   "id INTEGER PRIMARY KEY AUTOINCREMENT,"
                   "name TEXT)",
                   NULL, NULL, NULL) != SQLITE_OK) {
    return false;
  }

  // Store all the add and sub chunk numbers we receive. We cannot just rely on
  // the prefix tables to generate these lists, since some chunks will have zero
  // entries (and thus no prefixes), or potentially an add chunk can have all of
  // its entries sub'd without receiving an AddDel, or a sub chunk might have
  // been entirely consumed by adds. In these cases, we still have to report the
  // chunk number but it will not have any prefixes in the prefix tables.
  //
  // TODO(paulg): Investigate storing the chunks as a string of ChunkRanges, one
  // string for each of phish-add, phish-sub, malware-add, malware-sub. This
  // might be better performance when the number of chunks is large, and is the
  // natural format for the update request.
  if (sqlite3_exec(db_, "CREATE TABLE add_chunks ("
                   "chunk INTEGER PRIMARY KEY)",
                   NULL, NULL, NULL) != SQLITE_OK) {
    return false;
  }

  if (sqlite3_exec(db_, "CREATE TABLE sub_chunks ("
                   "chunk INTEGER PRIMARY KEY)",
                   NULL, NULL, NULL) != SQLITE_OK) {
    return false;
  }

  std::string version = "PRAGMA user_version=";
  version += StringPrintf("%d", kDatabaseVersion);

  SQLITE_UNIQUE_STATEMENT(statement, *statement_cache_, version.c_str());
  if (!statement.is_valid()) {
    NOTREACHED();
    return false;
  }

  if (statement->step() != SQLITE_DONE)
    return false;

  transaction.Commit();
  add_count_ = 0;
  return true;
}

// The SafeBrowsing service assumes this operation is run synchronously on the
// database thread. Any URLs that the service needs to check when this is
// running are queued up and run once the reset is done.
bool SafeBrowsingDatabaseBloom::ResetDatabase() {
  hash_cache_.clear();
  ClearUpdateCaches();

  bool rv = Close();
  DCHECK(rv);

  if (!file_util::Delete(filename_, false)) {
    NOTREACHED();
    return false;
  }

  bloom_filter_ =
      new BloomFilter(kBloomFilterMinSize * kBloomFilterSizeRatio);
  file_util::Delete(bloom_filter_filename_, false);

  if (!Open())
    return false;

  return CreateTables();
}

bool SafeBrowsingDatabaseBloom::CheckCompatibleVersion() {
  SQLITE_UNIQUE_STATEMENT(statement, *statement_cache_,
      "PRAGMA user_version");
  if (!statement.is_valid()) {
    NOTREACHED();
    return false;
  }

  int result = statement->step();
  if (result != SQLITE_ROW)
    return false;

  return statement->column_int(0) == kDatabaseVersion;
}

void SafeBrowsingDatabaseBloom::ClearUpdateCaches() {
  add_del_cache_.clear();
  sub_del_cache_.clear();
  add_chunk_cache_.clear();
  sub_chunk_cache_.clear();
  prefix_miss_cache_.clear();
}

bool SafeBrowsingDatabaseBloom::ContainsUrl(
    const GURL& url,
    std::string* matching_list,
    std::vector<SBPrefix>* prefix_hits,
    std::vector<SBFullHashResult>* full_hits,
    Time last_update) {

  // Clear the results first.
  matching_list->clear();
  prefix_hits->clear();
  full_hits->clear();

  std::vector<std::string> hosts;
  if (url.HostIsIPAddress()) {
    hosts.push_back(url.host());
  } else {
    safe_browsing_util::GenerateHostsToCheck(url, &hosts);
    if (hosts.size() == 0)
      return false;  // Things like about:blank
  }
  std::vector<std::string> paths;
  safe_browsing_util::GeneratePathsToCheck(url, &paths);

  // Grab a reference to the existing filter so that it isn't deleted on us if
  // a update is just about to finish.
  scoped_refptr<BloomFilter> filter = bloom_filter_;
  if (!filter.get())
    return false;

  // TODO(erikkay): This may wind up being too many hashes on a complex page.
  // TODO(erikkay): Not filling in matching_list - is that OK?
  for (size_t i = 0; i < hosts.size(); ++i) {
    for (size_t j = 0; j < paths.size(); ++j) {
      SBFullHash full_hash;
      // TODO(erikkay): Maybe we should only do the first 32 bits initially,
      // and then fall back to the full hash if there's a hit.
      base::SHA256HashString(hosts[i] + paths[j], &full_hash,
                             sizeof(SBFullHash));
      SBPrefix prefix;
      memcpy(&prefix, &full_hash, sizeof(SBPrefix));
      if (filter->Exists(prefix))
        prefix_hits->push_back(prefix);
    }
  }

  if (!prefix_hits->empty()) {
    // If all the prefixes are cached as 'misses', don't issue a GetHash.
    bool all_misses = true;
    for (std::vector<SBPrefix>::const_iterator it = prefix_hits->begin();
         it != prefix_hits->end(); ++it) {
      if (prefix_miss_cache_.find(*it) == prefix_miss_cache_.end()) {
        all_misses = false;
        break;
      }
    }
    if (all_misses)
      return false;

    // See if we have the results of recent GetHashes for the prefix matches.
    GetCachedFullHashes(prefix_hits, full_hits, last_update);
    return true;
  }

  return false;
}

bool SafeBrowsingDatabaseBloom::NeedToCheckUrl(const GURL& url) {
  // Since everything is in the bloom filter, doing anything here would wind
  // up just duplicating work that would happen in ContainsURL.
  // It's possible that we may want to add a hostkey-based first-level cache
  // on the front of this to minimize hash generation, but we'll need to do
  // some measurements to verify that.
  return true;
}

void SafeBrowsingDatabaseBloom::InsertChunks(const std::string& list_name,
                                             std::deque<SBChunk>* chunks) {
  // We've going to be updating the bloom filter, so delete the on-disk
  // serialization so that if the process crashes we'll generate a new one on
  // startup, instead of reading a stale filter.
  // TODO(erikkay) - is this correct?  Since we can no longer fall back to
  // database lookups, we need a reasonably current bloom filter at startup.
  // I think we need some way to indicate that the bloom filter is out of date
  // and needs to be rebuilt, but we shouldn't delete it.
  // DeleteBloomFilter();
  if (chunks->empty())
    return;

  int list_id = GetListID(list_name);
  ChunkType chunk_type = chunks->front().is_add ? ADD_CHUNK : SUB_CHUNK;

  while (!chunks->empty()) {
    SBChunk& chunk = chunks->front();
    chunk.list_id = list_id;
    int chunk_id = chunk.chunk_number;

    // The server can give us a chunk that we already have because it's part of
    // a range.  Don't add it again.
    if (!ChunkExists(list_id, chunk_type, chunk_id)) {
      while (!chunk.hosts.empty()) {
        // Read the existing record for this host, if it exists.
        SBPrefix host = chunk.hosts.front().host;
        SBEntry* entry = chunk.hosts.front().entry;
        entry->set_list_id(list_id);
        if (chunk_type == ADD_CHUNK) {
          entry->set_chunk_id(chunk_id);
          AddEntry(host, entry);
        } else {
          AddSub(chunk_id, host, entry);
        }

        entry->Destroy();
        chunk.hosts.pop_front();
      }

      int encoded = EncodeChunkId(chunk_id, list_id);
      if (chunk_type == ADD_CHUNK)
        add_chunk_cache_.insert(encoded);
      else
        sub_chunk_cache_.insert(encoded);
    }

    chunks->pop_front();
  }

  delete chunks;

  if (chunk_inserted_callback_.get())
    chunk_inserted_callback_->Run();
}

void SafeBrowsingDatabaseBloom::UpdateFinished(bool update_succeeded) {
  if (update_succeeded)
    BuildBloomFilter();

  // We won't need the chunk caches until the next update (which will read them
  // from the database), so free their memory as they may contain thousands of
  // entries.
  ClearUpdateCaches();
}

void SafeBrowsingDatabaseBloom::AddEntry(SBPrefix host, SBEntry* entry) {
  STATS_COUNTER(L"SB.HostInsert", 1);
  if (entry->type() == SBEntry::ADD_FULL_HASH) {
    // TODO(erikkay, paulg): Add the full 256 bit prefix to the full_prefix
    // table AND insert its 32 bit prefix into the regular prefix table.
    return;
  }
  int encoded = EncodeChunkId(entry->chunk_id(), entry->list_id());
  int count = entry->prefix_count();
  if (count == 0) {
    AddPrefix(host, encoded);
  } else {
    for (int i = 0; i < count; i++) {
      SBPrefix prefix = entry->PrefixAt(i);
      AddPrefix(prefix, encoded);
    }
  }
}

void SafeBrowsingDatabaseBloom::AddPrefix(SBPrefix prefix, int encoded_chunk) {
  STATS_COUNTER(L"SB.PrefixAdd", 1);
  std::string sql = "INSERT INTO add_prefix (chunk, prefix) VALUES (?, ?)";
  SQLITE_UNIQUE_STATEMENT(statement, *statement_cache_, sql.c_str());
  if (!statement.is_valid()) {
    NOTREACHED();
    return;
  }
  statement->bind_int(0, encoded_chunk);
  statement->bind_int(1, prefix);
  int rv = statement->step();
  statement->reset();
  if (rv == SQLITE_CORRUPT) {
    HandleCorruptDatabase();
  } else {
    DCHECK(rv == SQLITE_DONE);
  }
  bloom_filter_->Insert(prefix);
  add_count_++;
}

void SafeBrowsingDatabaseBloom::AddFullPrefix(SBPrefix prefix,
                                              int encoded_chunk,
                                              SBFullHash full_prefix) {
  STATS_COUNTER(L"SB.PrefixAddFull", 1);
  std::string sql = "INSERT INTO full_add_prefix "
                    "(chunk, prefix, receive_time, full_prefix) "
                    "VALUES (?, ?, ?)";
  SQLITE_UNIQUE_STATEMENT(statement, *statement_cache_, sql.c_str());
  if (!statement.is_valid()) {
    NOTREACHED();
    return;
  }
  statement->bind_int(0, encoded_chunk);
  statement->bind_int(1, prefix);
  // TODO(paulg): Add receive_time and full_prefix.
  // statement->bind_int64(2, receive_time);
  // statement->bind_blob(3, full_prefix);
  int rv = statement->step();
  statement->reset();
  if (rv == SQLITE_CORRUPT) {
    HandleCorruptDatabase();
  } else {
    DCHECK(rv == SQLITE_DONE);
  }
}

void SafeBrowsingDatabaseBloom::AddSub(
    int chunk_id, SBPrefix host, SBEntry* entry) {
  STATS_COUNTER(L"SB.HostDelete", 1);
  if (entry->type() == SBEntry::SUB_FULL_HASH) {
    // TODO(erikkay)
    return;
  }

  int encoded = EncodeChunkId(chunk_id, entry->list_id());
  int encoded_add = EncodeChunkId(entry->chunk_id(), entry->list_id());
  int count = entry->prefix_count();
  if (count == 0) {
    AddSubPrefix(host, encoded, encoded_add);
  } else {
    for (int i = 0; i < count; i++) {
      SBPrefix prefix = entry->PrefixAt(i);
      AddSubPrefix(prefix, encoded, encoded_add);
    }
  }
}

void SafeBrowsingDatabaseBloom::AddSubPrefix(SBPrefix prefix,
                                             int encoded_chunk,
                                             int encoded_add_chunk) {
  STATS_COUNTER(L"SB.PrefixSub", 1);
  std::string sql =
    "INSERT INTO sub_prefix (chunk, add_chunk, prefix) VALUES (?,?,?)";
  SQLITE_UNIQUE_STATEMENT(statement, *statement_cache_, sql.c_str());
  if (!statement.is_valid()) {
    NOTREACHED();
    return;
  }
  statement->bind_int(0, encoded_chunk);
  statement->bind_int(1, encoded_add_chunk);
  statement->bind_int(2, prefix);
  int rv = statement->step();
  statement->reset();
  if (rv == SQLITE_CORRUPT) {
    HandleCorruptDatabase();
  } else {
    DCHECK(rv == SQLITE_DONE);
  }
}

void SafeBrowsingDatabaseBloom::SubFullPrefix(SBPrefix prefix,
                                              int encoded_chunk,
                                              int encoded_add_chunk,
                                              SBFullHash full_prefix) {
  STATS_COUNTER(L"SB.PrefixSubFull", 1);
  std::string sql = "INSERT INTO full_sub_prefix "
                    "(chunk, add_chunk, prefix, receive_time, full_prefix) "
                    "VALUES (?,?,?,?)";
  SQLITE_UNIQUE_STATEMENT(statement, *statement_cache_, sql.c_str());
  if (!statement.is_valid()) {
    NOTREACHED();
    return;
  }
  statement->bind_int(0, encoded_chunk);
  statement->bind_int(1, encoded_add_chunk);
  statement->bind_int(2, prefix);
  // TODO(paulg): Add receive_time and full_prefix.
  // statement->bind_int64(3, receive_time);
  // statement->bind_blob(4, full_prefix);
  int rv = statement->step();
  statement->reset();
  if (rv == SQLITE_CORRUPT) {
    HandleCorruptDatabase();
  } else {
    DCHECK(rv == SQLITE_DONE);
  }
}

// TODO(paulg): Look for a less expensive way to maintain add_count_.
int SafeBrowsingDatabaseBloom::GetAddPrefixCount() {
  SQLITE_UNIQUE_STATEMENT(count, *statement_cache_,
      "SELECT count(*) FROM add_prefix");
  if (!count.is_valid()) {
    NOTREACHED();
    return 0;
  }
  int rv = count->step();
  int add_count = 0;
  if (rv == SQLITE_ROW)
    add_count = count->column_int(0);
  else if (rv == SQLITE_CORRUPT)
    HandleCorruptDatabase();

  return add_count;
}

void SafeBrowsingDatabaseBloom::DeleteChunks(
    std::vector<SBChunkDelete>* chunk_deletes) {
  if (chunk_deletes->empty())
    return;

  int list_id = GetListID(chunk_deletes->front().list_name);

  for (size_t i = 0; i < chunk_deletes->size(); ++i) {
    const SBChunkDelete& chunk = (*chunk_deletes)[i];
    std::vector<int> chunk_numbers;
    RangesToChunks(chunk.chunk_del, &chunk_numbers);
    for (size_t del = 0; del < chunk_numbers.size(); ++del) {
      int encoded_chunk = EncodeChunkId(chunk_numbers[del], list_id);
      if (chunk.is_sub_del)
        sub_del_cache_.insert(encoded_chunk);
      else
        add_del_cache_.insert(encoded_chunk);
    }
  }

  delete chunk_deletes;
}

bool SafeBrowsingDatabaseBloom::ChunkExists(int list_id,
                                            ChunkType type,
                                            int chunk_id) {
  STATS_COUNTER(L"SB.ChunkSelect", 1);
  int encoded = EncodeChunkId(chunk_id, list_id);
  bool ret;
  if (type == ADD_CHUNK)
    ret = add_chunk_cache_.count(encoded) > 0;
  else
    ret = sub_chunk_cache_.count(encoded) > 0;

  return ret;
}

// Return a comma separated list of chunk ids that are in the database for
// the given list and chunk type.
void SafeBrowsingDatabaseBloom::GetChunkIds(
    int list_id, ChunkType type, std::string* list) {

  std::set<int>::iterator i, end;
  if (type == ADD_CHUNK) {
    i = add_chunk_cache_.begin();
    end = add_chunk_cache_.end();
  } else {
    i = sub_chunk_cache_.begin();
    end = sub_chunk_cache_.end();
  }
  std::vector<int> chunks;
  for (; i != end; ++i) {
    int chunk = *i;
    int list_id2;
    DecodeChunkId(chunk, &chunk, &list_id2);
    if (list_id2 == list_id)
      chunks.push_back(chunk);
  }
  std::vector<ChunkRange> ranges;
  ChunksToRanges(chunks, &ranges);
  RangesToString(ranges, list);
}

void SafeBrowsingDatabaseBloom::GetListsInfo(
    std::vector<SBListChunkRanges>* lists) {
  DCHECK(lists);

  ReadChunkNumbers();

  lists->clear();
  SQLITE_UNIQUE_STATEMENT(statement, *statement_cache_,
      "SELECT name,id FROM list_names");
  if (!statement.is_valid()) {
    NOTREACHED();
    return;
  }

  std::vector<std::vector<int> > chunks;
  while (true) {
    int rv = statement->step();
    if (rv != SQLITE_ROW) {
      if (rv == SQLITE_CORRUPT)
        HandleCorruptDatabase();

      break;
    }
    int list_id = statement->column_int(1);
    lists->push_back(SBListChunkRanges(statement->column_string(0)));
    std::vector<int> c;
    chunks.push_back(c);
    GetChunkIds(list_id, ADD_CHUNK, &lists->back().adds);
    GetChunkIds(list_id, SUB_CHUNK, &lists->back().subs);
  }

  return;
}

int SafeBrowsingDatabaseBloom::AddList(const std::string& name) {
  SQLITE_UNIQUE_STATEMENT(statement, *statement_cache_,
      "INSERT INTO list_names"
      "(id,name)"
      "VALUES (NULL,?)");
  if (!statement.is_valid()) {
    NOTREACHED();
    return 0;
  }

  statement->bind_string(0, name);
  int rv = statement->step();
  if (rv != SQLITE_DONE) {
    if (rv == SQLITE_CORRUPT) {
      HandleCorruptDatabase();
    } else {
      NOTREACHED();
    }

    return 0;
  }

  return static_cast<int>(sqlite3_last_insert_rowid(db_));
}

int SafeBrowsingDatabaseBloom::GetListID(const std::string& name) {
  SQLITE_UNIQUE_STATEMENT(statement, *statement_cache_,
      "SELECT id FROM list_names WHERE name=?");
  if (!statement.is_valid()) {
    NOTREACHED();
    return 0;
  }

  statement->bind_string(0, name);
  int result = statement->step();
  if (result == SQLITE_ROW)
    return statement->column_int(0);

  if (result == SQLITE_CORRUPT)
    HandleCorruptDatabase();

  // There isn't an existing entry so add one.
  return AddList(name);
}

std::string SafeBrowsingDatabaseBloom::GetListName(int id) {
  SQLITE_UNIQUE_STATEMENT(statement, *statement_cache_,
      "SELECT name FROM list_names WHERE id=?");
  if (!statement.is_valid()) {
    NOTREACHED();
    return 0;
  }

  statement->bind_int(0, id);
  int result = statement->step();
  if (result != SQLITE_ROW) {
    if (result == SQLITE_CORRUPT)
      HandleCorruptDatabase();

    return std::string();
  }

  return statement->column_string(0);
}

void SafeBrowsingDatabaseBloom::ReadChunkNumbers() {
  add_chunk_cache_.clear();
  sub_chunk_cache_.clear();

  // Read in the add chunk numbers.
  SQLITE_UNIQUE_STATEMENT(read_adds, *statement_cache_,
                          "SELECT chunk FROM add_chunks");
  if (!read_adds.is_valid()) {
    NOTREACHED();
    return;
  }

  while (true) {
    int rv = read_adds->step();
    if (rv != SQLITE_ROW) {
      if (rv == SQLITE_CORRUPT)
        HandleCorruptDatabase();
      break;
    }
    add_chunk_cache_.insert(read_adds->column_int(0));
  }

  // Read in the sub chunk numbers.
  SQLITE_UNIQUE_STATEMENT(read_subs, *statement_cache_,
                          "SELECT chunk FROM sub_chunks");
  if (!read_subs.is_valid()) {
    NOTREACHED();
    return;
  }

  while (true) {
    int rv = read_subs->step();
    if (rv != SQLITE_ROW) {
      if (rv == SQLITE_CORRUPT)
        HandleCorruptDatabase();
      break;
    }
    sub_chunk_cache_.insert(read_subs->column_int(0));
  }
}

// Write all the chunk numbers to the add_chunks and sub_chunks tables.
bool SafeBrowsingDatabaseBloom::WriteChunkNumbers() {
  // Delete the contents of the add chunk table.
  SQLITE_UNIQUE_STATEMENT(del_add_chunk, *statement_cache_,
                          "DELETE FROM add_chunks");
  if (!del_add_chunk.is_valid()) {
    NOTREACHED();
    return false;
  }
  int rv = del_add_chunk->step();
  if (rv == SQLITE_CORRUPT) {
    HandleCorruptDatabase();
    return false;
  }
  DCHECK(rv == SQLITE_DONE);

  SQLITE_UNIQUE_STATEMENT(write_adds, *statement_cache_,
                          "INSERT INTO add_chunks (chunk) VALUES (?)");
  if (!write_adds.is_valid()) {
    NOTREACHED();
    return false;
  }

  // Write all the add chunks from the cache to the database.
  std::set<int>::const_iterator it = add_chunk_cache_.begin();
  for (; it != add_chunk_cache_.end(); ++it) {
    if (add_del_cache_.find(*it) != add_del_cache_.end())
      continue;  // This chunk has been deleted.
    write_adds->bind_int(0, *it);
    rv = write_adds->step();
    if (rv == SQLITE_CORRUPT) {
      HandleCorruptDatabase();
      return false;
    }
    DCHECK(rv == SQLITE_DONE);
    write_adds->reset();
  }

  // Delete the contents of the sub chunk table.
  SQLITE_UNIQUE_STATEMENT(del_sub_chunk, *statement_cache_,
                          "DELETE FROM sub_chunks");
  if (!del_sub_chunk.is_valid()) {
    NOTREACHED();
    return false;
  }
  rv = del_sub_chunk->step();
  if (rv == SQLITE_CORRUPT) {
    HandleCorruptDatabase();
    return false;
  }
  DCHECK(rv == SQLITE_DONE);

  SQLITE_UNIQUE_STATEMENT(write_subs, *statement_cache_,
                          "INSERT INTO sub_chunks (chunk) VALUES (?)");
  if (!write_subs.is_valid()) {
    NOTREACHED();
    return false;
  }

  // Write all the sub chunks from the cache to the database.
  it = sub_chunk_cache_.begin();
  for (; it != sub_chunk_cache_.end(); ++it) {
    if (sub_del_cache_.find(*it) != sub_del_cache_.end())
      continue;  // This chunk has been deleted.
    write_subs->bind_int(0, *it);
    rv = write_subs->step();
    if (rv == SQLITE_CORRUPT) {
      HandleCorruptDatabase();
      return false;
    }
    DCHECK(rv == SQLITE_DONE);
    write_subs->reset();
  }

  return true;
}

int SafeBrowsingDatabaseBloom::PairCompare(const void* arg1, const void* arg2) {
  const SBPair* p1 = reinterpret_cast<const SBPair*>(arg1);
  const SBPair* p2 = reinterpret_cast<const SBPair*>(arg2);
  int delta = p1->chunk_id - p2->chunk_id;
  if (delta == 0)
    delta = p1->prefix - p2->prefix;
  return delta;
}

bool SafeBrowsingDatabaseBloom::BuildAddList(SBPair* adds) {
  // Read add_prefix into memory and sort it.
  STATS_COUNTER(L"SB.HostSelectForBloomFilter", 1);
  SQLITE_UNIQUE_STATEMENT(add_prefix, *statement_cache_,
      "SELECT chunk, prefix FROM add_prefix");
  if (!add_prefix.is_valid()) {
    NOTREACHED();
    return false;
  }

  SBPair* add = adds;
  while (true) {
    int rv = add_prefix->step();
    if (rv != SQLITE_ROW) {
      if (rv == SQLITE_CORRUPT)
        HandleCorruptDatabase();
      break;
    }
    add->chunk_id = add_prefix->column_int(0);
    add->prefix = add_prefix->column_int(1);
    add++;
    if (add_count_ < (add - adds))
      break;
  }
  DCHECK(add_count_ == (add - adds));
  qsort(adds, add_count_, sizeof(SBPair),
        &SafeBrowsingDatabaseBloom::PairCompare);

  return true;
}

bool SafeBrowsingDatabaseBloom::RemoveSubs(SBPair* adds,
                                           std::vector<bool>* adds_removed) {
  // Read through sub_prefix and zero out add_prefix entries that match.
  SQLITE_UNIQUE_STATEMENT(sub_prefix, *statement_cache_,
      "SELECT chunk, add_chunk, prefix FROM sub_prefix");
  if (!sub_prefix.is_valid()) {
    NOTREACHED();
    return false;
  }

  // Create a temporary sub prefix table. We add entries to it as we scan the
  // sub_prefix table looking for adds to remove. Only entries that don't
  // remove an add written to this table. When we're done filtering, we replace
  // sub_prefix with this table.
  if (sqlite3_exec(db_, "CREATE TABLE sub_prefix_tmp ("
                   "chunk INTEGER,"
                   "add_chunk INTEGER,"
                   "prefix INTEGER)",
                   NULL, NULL, NULL) != SQLITE_OK) {
    return false;
  }

  SQLITE_UNIQUE_STATEMENT(
      sub_prefix_tmp,
      *statement_cache_,
      "INSERT INTO sub_prefix_tmp (chunk,add_chunk,prefix) VALUES (?,?,?)");
  if (!sub_prefix_tmp.is_valid()) {
    NOTREACHED();
    return false;
  }

  SBPair sub;
  while (true) {
    int rv = sub_prefix->step();
    if (rv != SQLITE_ROW) {
      if (rv == SQLITE_CORRUPT) {
        HandleCorruptDatabase();
        return false;
      }
      break;
    }

    int sub_chunk = sub_prefix->column_int(0);
    sub.chunk_id = sub_prefix->column_int(1);
    sub.prefix = sub_prefix->column_int(2);

    // See if this sub chunk has been deleted via a SubDel, and skip doing the
    // search or write if so.
    if (sub_del_cache_.find(sub_chunk) != sub_del_cache_.end())
      continue;

    void* match = bsearch(&sub, adds, add_count_, sizeof(SBPair), 
                          &SafeBrowsingDatabaseBloom::PairCompare);
    if (match) {
      SBPair* subbed = reinterpret_cast<SBPair*>(match);
      (*adds_removed)[subbed - adds] = true;
    } else {
      // This sub_prefix entry did not match any add, so we keep it around.
      sub_prefix_tmp->bind_int(0, sub_chunk);
      sub_prefix_tmp->bind_int(1, sub.chunk_id);
      sub_prefix_tmp->bind_int(2, sub.prefix);
      int rv = sub_prefix_tmp->step();
      if (rv == SQLITE_CORRUPT) {
        HandleCorruptDatabase();
        return false;
      }
      DCHECK(rv == SQLITE_DONE);
      sub_prefix_tmp->reset();
    }
  }
  
  return true;
}

bool SafeBrowsingDatabaseBloom::UpdateTables() {
  // Delete the old sub_prefix table and rename the temporary table.
  SQLITE_UNIQUE_STATEMENT(del_sub, *statement_cache_, "DROP TABLE sub_prefix");
  if (!del_sub.is_valid()) {
    NOTREACHED();
    return false;
  }

  int rv = del_sub->step();
  if (rv == SQLITE_CORRUPT) {
    HandleCorruptDatabase();
    return false;
  }
  DCHECK(rv == SQLITE_DONE);

  SQLITE_UNIQUE_STATEMENT(rename_sub, *statement_cache_,
                          "ALTER TABLE sub_prefix_tmp RENAME TO sub_prefix");
  if (!rename_sub.is_valid()) {
    NOTREACHED();
    return false;
  }
  rv = rename_sub->step();
  if (rv == SQLITE_CORRUPT) {
    HandleCorruptDatabase();
    return false;
  }
  DCHECK(rv == SQLITE_DONE);

  // Now blow away add_prefix and re-write it from our in-memory data,
  // building the new bloom filter as we go.
  SQLITE_UNIQUE_STATEMENT(del_add, *statement_cache_, "DELETE FROM add_prefix");
  if (!del_add.is_valid()) {
    NOTREACHED();
    return false;
  }
  rv = del_add->step();
  if (rv == SQLITE_CORRUPT) {
    HandleCorruptDatabase();
    return false;
  }
  DCHECK(rv == SQLITE_DONE);

  return true;
}

bool SafeBrowsingDatabaseBloom::WritePrefixes(
    SBPair* adds,
    const std::vector<bool>& adds_removed,
    int* new_add_count,
    BloomFilter** filter) {
  *filter = NULL;
  *new_add_count = 0;

  SQLITE_UNIQUE_STATEMENT(insert, *statement_cache_,
                          "INSERT INTO add_prefix VALUES (?,?)");
  if (!insert.is_valid()) {
    NOTREACHED();
    return false;
  }

  int number_of_keys = std::max(add_count_, kBloomFilterMinSize);
  int filter_size = number_of_keys * kBloomFilterSizeRatio;
  BloomFilter* new_filter = new BloomFilter(filter_size);
  SBPair* add = adds;
  int new_count = 0;

  while (add - adds < add_count_) {
    if (!adds_removed[add - adds]) {
      // Check to see if we have an AddDel for this chunk and skip writing it
      // if there is.
      if (add_del_cache_.find(add->chunk_id) != add_del_cache_.end()) {
        add++;
        continue;
      }
      new_filter->Insert(add->prefix);
      insert->bind_int(0, add->chunk_id);
      insert->bind_int(1, add->prefix);
      int rv = insert->step();
      if (rv == SQLITE_CORRUPT) {
        HandleCorruptDatabase();
        delete new_filter;  // TODO(paulg): scoped.
        return false;
      }
      DCHECK(rv == SQLITE_DONE);
      insert->reset();
      new_count++;
    }
    add++;
  }

  *new_add_count = new_count;
  *filter = new_filter;

  return true;
}

// TODO(erikkay): should we call WaitAfterResume() inside any of the loops here?
// This is a pretty fast operation and it would be nice to let it finish.
void SafeBrowsingDatabaseBloom::BuildBloomFilter() {
  Time before = Time::Now();

  add_count_ = GetAddPrefixCount();
  if (add_count_ == 0) {
    bloom_filter_ = NULL;
    return;
  }

  scoped_array<SBPair> adds_array(new SBPair[add_count_]);
  SBPair* adds = adds_array.get();

  if (!BuildAddList(adds))
    return;

  // Protect the remaining operations on the database with a transaction.
  scoped_ptr<SQLTransaction> update_transaction;
  update_transaction.reset(new SQLTransaction(db_));
  if (update_transaction->Begin() != SQLITE_OK) {
    NOTREACHED();
    return;
  }

  // Used to track which adds have been subbed out. The vector<bool> is actually
  // a bitvector so the size is as small as we can get.
  std::vector<bool> adds_removed;
  adds_removed.resize(add_count_, false);

  // Flag any add as removed if there is a matching sub.
  if (!RemoveSubs(adds, &adds_removed))
    return;

  // Prepare the database for writing out our remaining add and sub prefixes.
  if (!UpdateTables())
    return;

  // Write out the remaining adds to the filter and database.
  int new_count;
  BloomFilter* filter;
  if (!WritePrefixes(adds, adds_removed, &new_count, &filter))
    return;

  // If there were any matching subs, the size will be smaller.
  add_count_ = new_count;
  bloom_filter_ = filter;

  TimeDelta bloom_gen = Time::Now() - before;
  SB_DLOG(INFO) << "SafeBrowsingDatabaseImpl built bloom filter in "
                << bloom_gen.InMilliseconds()
                << " ms total.  prefix count: "<< add_count_;
  UMA_HISTOGRAM_LONG_TIMES(L"SB.BuildBloom", bloom_gen);

  // Save the chunk numbers we've received to the database for reporting in
  // future update requests.
  if (!WriteChunkNumbers())
    return;

  // We've made it this far without errors, so commit the changes.
  update_transaction->Commit();

  // Persist the bloom filter to disk.
  WriteBloomFilter();
}

void SafeBrowsingDatabaseBloom::GetCachedFullHashes(
    const std::vector<SBPrefix>* prefix_hits,
    std::vector<SBFullHashResult>* full_hits,
    Time last_update) {
  DCHECK(prefix_hits && full_hits);

  Time max_age = Time::Now() - TimeDelta::FromMinutes(kMaxStalenessMinutes);

  for (std::vector<SBPrefix>::const_iterator it = prefix_hits->begin();
       it != prefix_hits->end(); ++it) {
    HashCache::iterator hit = hash_cache_.find(*it);
    if (hit != hash_cache_.end()) {
      HashList& entries = hit->second;
      HashList::iterator eit = entries.begin();
      while (eit != entries.end()) {
        // An entry is valid if we've received an update in the past 45 minutes,
        // or if this particular GetHash was received in the past 45 minutes.
        if (max_age < last_update || eit->received > max_age) {
          SBFullHashResult full_hash;
          memcpy(&full_hash.hash.full_hash,
                 &eit->full_hash.full_hash,
                 sizeof(SBFullHash));
          full_hash.list_name = GetListName(eit->list_id);
          full_hash.add_chunk_id = eit->add_chunk_id;
          full_hits->push_back(full_hash);
          ++eit;
        } else {
          // Evict the expired entry.
          eit = entries.erase(eit);
        }
      }

      if (entries.empty())
        hash_cache_.erase(hit);
    }
  }
}

void SafeBrowsingDatabaseBloom::CacheHashResults(
    const std::vector<SBPrefix>& prefixes,
    const std::vector<SBFullHashResult>& full_hits) {
  if (full_hits.empty()) {
    // These prefixes returned no results, so we store them in order to prevent
    // asking for them again. We flush this cache at the next update.
    for (std::vector<SBPrefix>::const_iterator it = prefixes.begin();
         it != prefixes.end(); ++it) {
      prefix_miss_cache_.insert(*it);
    }
    return;
  }

  const Time now = Time::Now();
  for (std::vector<SBFullHashResult>::const_iterator it = full_hits.begin();
       it != full_hits.end(); ++it) {
    SBPrefix prefix;
    memcpy(&prefix, &it->hash.full_hash, sizeof(prefix));
    HashList& entries = hash_cache_[prefix];
    HashCacheEntry entry;
    entry.received = now;
    entry.list_id = GetListID(it->list_name);
    entry.add_chunk_id = it->add_chunk_id;
    memcpy(&entry.full_hash, &it->hash.full_hash, sizeof(SBFullHash));
    entries.push_back(entry);
  }
}

void SafeBrowsingDatabaseBloom::ClearCachedHashes(const SBEntry* entry) {
  for (int i = 0; i < entry->prefix_count(); ++i) {
    SBPrefix prefix;
    if (entry->type() == SBEntry::SUB_FULL_HASH)
      memcpy(&prefix, &entry->FullHashAt(i), sizeof(SBPrefix));
    else
      prefix = entry->PrefixAt(i);

    HashCache::iterator it = hash_cache_.find(prefix);
    if (it != hash_cache_.end())
      hash_cache_.erase(it);
  }
}

// This clearing algorithm is a little inefficient, but we don't expect there to
// be too many entries for this to matter. Also, this runs as a background task
// during an update, so no user action is blocking on it.
void SafeBrowsingDatabaseBloom::ClearCachedHashesForChunk(int list_id,
                                                         int add_chunk_id) {
  HashCache::iterator it = hash_cache_.begin();
  while (it != hash_cache_.end()) {
    HashList& entries = it->second;
    HashList::iterator eit = entries.begin();
    while (eit != entries.end()) {
      if (eit->list_id == list_id && eit->add_chunk_id == add_chunk_id)
        eit = entries.erase(eit);
      else
        ++eit;
    }
    if (entries.empty())
      hash_cache_.erase(it++);
    else
      ++it;
  }
}

void SafeBrowsingDatabaseBloom::HandleCorruptDatabase() {
  MessageLoop::current()->PostTask(FROM_HERE,
      reset_factory_.NewRunnableMethod(
          &SafeBrowsingDatabaseBloom::OnHandleCorruptDatabase));
}

void SafeBrowsingDatabaseBloom::OnHandleCorruptDatabase() {
  ResetDatabase();
  DCHECK(false) << "SafeBrowsing database was corrupt and reset";
}

void SafeBrowsingDatabaseBloom::HandleResume() {
  did_resume_ = true;
  MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      resume_factory_.NewRunnableMethod(
          &SafeBrowsingDatabaseBloom::OnResumeDone),
      kOnResumeHoldupMs);
}

void SafeBrowsingDatabaseBloom::OnResumeDone() {
  did_resume_ = false;
}

void SafeBrowsingDatabaseBloom::WaitAfterResume() {
  if (did_resume_) {
    PlatformThread::Sleep(kOnResumeHoldupMs);
    did_resume_ = false;
  }
}

// This database is always synchronous since we don't need to worry about
// blocking any incoming reads.
void SafeBrowsingDatabaseBloom::SetSynchronous() {
}
