// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/safe_browsing_database_bloom.h"

#include "base/compiler_specific.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/platform_thread.h"
#include "base/process_util.h"
#include "base/sha2.h"
#include "base/stats_counters.h"
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

// The bloom filter based file name suffix.
static const wchar_t kBloomFilterFileSuffix[] = L" Bloom";


// Implementation --------------------------------------------------------------

SafeBrowsingDatabaseBloom::SafeBrowsingDatabaseBloom()
    : db_(NULL),
      init_(false),
      ALLOW_THIS_IN_INITIALIZER_LIST(reset_factory_(this)),
      ALLOW_THIS_IN_INITIALIZER_LIST(resume_factory_(this)),
      add_count_(0),
      did_resume_(false) {
}

SafeBrowsingDatabaseBloom::~SafeBrowsingDatabaseBloom() {
  Close();
}

bool SafeBrowsingDatabaseBloom::Init(const std::wstring& filename,
                                     Callback0::Type* chunk_inserted_callback) {
  DCHECK(!init_ && filename_.empty());

  filename_ = filename + kBloomFilterFileSuffix;
  bloom_filter_filename_ = BloomFilterFilename(filename_);

  hash_cache_.reset(new HashCache);

  LoadBloomFilter();

  init_ = true;
  chunk_inserted_callback_.reset(chunk_inserted_callback);

  return true;
}

void SafeBrowsingDatabaseBloom::LoadBloomFilter() {
  DCHECK(!bloom_filter_filename_.empty());

  // If we're missing either of the database or filter files, we wait until the
  // next update to generate a new filter.
  // TODO(paulg): Investigate how often the filter file is missing and how
  // expensive it would be to regenerate it.
  int64 size_64;
  if (!file_util::GetFileSize(filename_, &size_64) || size_64 == 0)
    return;

  if (!file_util::GetFileSize(bloom_filter_filename_, &size_64) ||
      size_64 == 0) {
    UMA_HISTOGRAM_COUNTS(L"SB2.FilterMissing", 1);
    return;
  }

  // We have a bloom filter file, so use that as our filter.
  int size = static_cast<int>(size_64);
  char* data = new char[size];
  CHECK(data);

  Time before = Time::Now();
  file_util::ReadFile(bloom_filter_filename_, data, size);
  SB_DLOG(INFO) << "SafeBrowsingDatabase read bloom filter in "
                << (Time::Now() - before).InMilliseconds() << " ms";

  bloom_filter_ = new BloomFilter(data, size);
}

bool SafeBrowsingDatabaseBloom::Open() {
  if (db_)
    return true;

  if (sqlite3_open(WideToUTF8(filename_).c_str(), &db_) != SQLITE_OK) {
    sqlite3_close(db_);
    db_ = NULL;
    return false;
  }

  // Run the database in exclusive mode. Nobody else should be accessing the
  // database while we're running, and this will give somewhat improved perf.
  sqlite3_exec(db_, "PRAGMA locking_mode=EXCLUSIVE", NULL, NULL, NULL);

  statement_cache_.reset(new SqliteStatementCache(db_));

  if (!DoesSqliteTableExist(db_, "add_prefix")) {
    if (!CreateTables()) {
      // Database could be corrupt, try starting from scratch.
      if (!ResetDatabase())
        return false;
    }
  } else if (!CheckCompatibleVersion()) {
    if (!ResetDatabase())
      return false;
  }

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

  // Store 32 bit sub prefixes here.
  if (sqlite3_exec(db_, "CREATE TABLE sub_prefix ("
                   "chunk INTEGER,"
                   "add_chunk INTEGER,"
                   "prefix INTEGER)",
                   NULL, NULL, NULL) != SQLITE_OK) {
    return false;
  }

  // Store 256 bit add full hashes (and GetHash results) here.
  if (sqlite3_exec(db_, "CREATE TABLE add_full_hash ("
                   "chunk INTEGER,"
                   "prefix INTEGER,"
                   "receive_time INTEGER,"
                   "full_hash BLOB)",
                   NULL, NULL, NULL) != SQLITE_OK) {
    return false;
  }

  // Store 256 bit sub full hashes here.
  if (sqlite3_exec(db_, "CREATE TABLE sub_full_hash ("
                   "chunk INTEGER,"
                   "add_chunk INTEGER,"
                   "prefix INTEGER,"
                   "full_hash BLOB)",
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
  hash_cache_->clear();
  ClearUpdateCaches();

  insert_transaction_.reset();

  bool rv = Close();
  DCHECK(rv);

  if (!file_util::Delete(filename_, false)) {
    NOTREACHED();
    return false;
  }

  bloom_filter_ =
      new BloomFilter(kBloomFilterMinSize * kBloomFilterSizeRatio);
  file_util::Delete(bloom_filter_filename_, false);

  // TODO(paulg): Fix potential infinite recursion between Open and Reset.
  return Open();
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

  // Lock the bloom filter and cache so that they aren't deleted on us if an
  // update is just about to finish.
  AutoLock lock(lookup_lock_);

  if (!bloom_filter_.get())
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
      if (bloom_filter_->Exists(prefix))
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
  if (chunks->empty())
    return;

  base::Time insert_start = base::Time::Now();

  int list_id = safe_browsing_util::GetListId(list_name);
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
          InsertAdd(host, entry);
        } else {
          InsertSub(chunk_id, host, entry);
        }

        entry->Destroy();
        chunk.hosts.pop_front();
      }

      int encoded = EncodeChunkId(chunk_id, list_id);
      if (chunk_type == ADD_CHUNK)
        add_chunk_cache_.insert(encoded);
      else
        sub_chunk_cache_.insert(encoded);
    } else {
      while (!chunk.hosts.empty()) {
        chunk.hosts.front().entry->Destroy();
        chunk.hosts.pop_front();
      }
    }

    chunks->pop_front();
  }

  UMA_HISTOGRAM_TIMES(L"SB2.ChunkInsert", base::Time::Now() - insert_start);

  delete chunks;

  if (chunk_inserted_callback_.get())
    chunk_inserted_callback_->Run();
}

bool SafeBrowsingDatabaseBloom::UpdateStarted() {
  DCHECK(insert_transaction_.get() == NULL);

  if (!Open())
    return false;

  insert_transaction_.reset(new SQLTransaction(db_));
  if (insert_transaction_->Begin() != SQLITE_OK) {
    DCHECK(false) << "Safe browsing database couldn't start transaction";
    insert_transaction_.reset();
    Close();
    return false;
  }
  return true;
}

void SafeBrowsingDatabaseBloom::UpdateFinished(bool update_succeeded) {
  if (update_succeeded)
    BuildBloomFilter();

  insert_transaction_.reset();
  Close();

  // We won't need the chunk caches until the next update (which will read them
  // from the database), so free their memory as they may contain thousands of
  // entries.
  ClearUpdateCaches();
}

void SafeBrowsingDatabaseBloom::InsertAdd(SBPrefix host, SBEntry* entry) {
  STATS_COUNTER("SB.HostInsert", 1);
  int encoded = EncodeChunkId(entry->chunk_id(), entry->list_id());

  if (entry->type() == SBEntry::ADD_FULL_HASH) {
    base::Time receive_time = base::Time::Now();
    for (int i = 0; i < entry->prefix_count(); ++i) {
      SBPrefix prefix;
      SBFullHash full_hash = entry->FullHashAt(i);
      memcpy(&prefix, full_hash.full_hash, sizeof(SBPrefix));
      InsertAddPrefix(prefix, encoded);
      InsertAddFullHash(prefix, encoded, receive_time, full_hash);
    }
    return;
  }

  // This entry contains only regular (32 bit) prefixes.
  int count = entry->prefix_count();
  if (count == 0) {
    InsertAddPrefix(host, encoded);
  } else {
    for (int i = 0; i < count; i++) {
      SBPrefix prefix = entry->PrefixAt(i);
      InsertAddPrefix(prefix, encoded);
    }
  }
}

void SafeBrowsingDatabaseBloom::InsertAddPrefix(SBPrefix prefix,
                                                int encoded_chunk) {
  STATS_COUNTER("SB.PrefixAdd", 1);
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
  add_count_++;
}

void SafeBrowsingDatabaseBloom::InsertAddFullHash(SBPrefix prefix,
                                                  int encoded_chunk,
                                                  base::Time receive_time,
                                                  SBFullHash full_prefix) {
  STATS_COUNTER("SB.PrefixAddFull", 1);
  std::string sql = "INSERT INTO add_full_hash "
                    "(chunk, prefix, receive_time, full_hash) "
                    "VALUES (?,?,?,?)";
  SQLITE_UNIQUE_STATEMENT(statement, *statement_cache_, sql.c_str());
  if (!statement.is_valid()) {
    NOTREACHED();
    return;
  }

  statement->bind_int(0, encoded_chunk);
  statement->bind_int(1, prefix);
  statement->bind_int64(2, receive_time.ToTimeT());
  statement->bind_blob(3, full_prefix.full_hash, sizeof(SBFullHash));
  int rv = statement->step();
  statement->reset();
  if (rv == SQLITE_CORRUPT) {
    HandleCorruptDatabase();
  } else {
    DCHECK(rv == SQLITE_DONE);
  }
}

void SafeBrowsingDatabaseBloom::InsertSub(
    int chunk_id, SBPrefix host, SBEntry* entry) {
  STATS_COUNTER("SB.HostDelete", 1);
  int encoded = EncodeChunkId(chunk_id, entry->list_id());
  int encoded_add;

  if (entry->type() == SBEntry::SUB_FULL_HASH) {
    for (int i = 0; i < entry->prefix_count(); ++i) {
      SBPrefix prefix;
      SBFullHash full_hash = entry->FullHashAt(i);
      memcpy(&prefix, full_hash.full_hash, sizeof(SBPrefix));
      encoded_add = EncodeChunkId(entry->ChunkIdAtPrefix(i), entry->list_id());
      InsertSubPrefix(prefix, encoded, encoded_add);
      InsertSubFullHash(prefix, encoded, encoded_add, full_hash, false);
    }
  } else {
    // We have prefixes.
    int count = entry->prefix_count();
    if (count == 0) {
      encoded_add = EncodeChunkId(entry->chunk_id(), entry->list_id());
      InsertSubPrefix(host, encoded, encoded_add);
    } else {
      for (int i = 0; i < count; i++) {
        SBPrefix prefix = entry->PrefixAt(i);
        encoded_add = EncodeChunkId(entry->ChunkIdAtPrefix(i), entry->list_id());
        InsertSubPrefix(prefix, encoded, encoded_add);
      }
    }
  }
}

void SafeBrowsingDatabaseBloom::InsertSubPrefix(SBPrefix prefix,
                                                int encoded_chunk,
                                                int encoded_add_chunk) {
  STATS_COUNTER("SB.PrefixSub", 1);
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

void SafeBrowsingDatabaseBloom::InsertSubFullHash(SBPrefix prefix,
                                                  int encoded_chunk,
                                                  int encoded_add_chunk,
                                                  SBFullHash full_prefix,
                                                  bool use_temp_table) {
  STATS_COUNTER("SB.PrefixSubFull", 1);
  std::string sql = "INSERT INTO ";
  if (use_temp_table) {
    sql += "sub_full_tmp";
  } else {
    sql += "sub_full_hash";
  }
  sql += " (chunk, add_chunk, prefix, full_hash) VALUES (?,?,?,?)";

  SQLITE_UNIQUE_STATEMENT(statement, *statement_cache_, sql.c_str());
  if (!statement.is_valid()) {
    NOTREACHED();
    return;
  }
  statement->bind_int(0, encoded_chunk);
  statement->bind_int(1, encoded_add_chunk);
  statement->bind_int(2, prefix);
  statement->bind_blob(3, full_prefix.full_hash, sizeof(SBFullHash));
  int rv = statement->step();
  statement->reset();
  if (rv == SQLITE_CORRUPT) {
    HandleCorruptDatabase();
  } else {
    DCHECK(rv == SQLITE_DONE);
  }
}

void SafeBrowsingDatabaseBloom::ReadFullHash(SqliteCompiledStatement& statement,
                                             int column,
                                             SBFullHash* full_hash) {
  DCHECK(full_hash);
  std::vector<unsigned char> blob;
  statement->column_blob_as_vector(column, &blob);
  DCHECK(blob.size() == sizeof(SBFullHash));
  memcpy(full_hash->full_hash, &blob[0], sizeof(SBFullHash));
}

// TODO(paulg): Look for a less expensive way to maintain add_count_? If we move
// to a native file format, we can just cache the count in the file and not have
// to scan at all.
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

  int list_id = safe_browsing_util::GetListId(chunk_deletes->front().list_name);

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
  STATS_COUNTER("SB.ChunkSelect", 1);
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
  lists->clear();

  ReadChunkNumbers();

  lists->push_back(SBListChunkRanges(safe_browsing_util::kMalwareList));
  GetChunkIds(safe_browsing_util::MALWARE, ADD_CHUNK, &lists->back().adds);
  GetChunkIds(safe_browsing_util::MALWARE, SUB_CHUNK, &lists->back().subs);

  lists->push_back(SBListChunkRanges(safe_browsing_util::kPhishingList));
  GetChunkIds(safe_browsing_util::PHISH, ADD_CHUNK, &lists->back().adds);
  GetChunkIds(safe_browsing_util::PHISH, SUB_CHUNK, &lists->back().subs);

  return;
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

bool SafeBrowsingDatabaseBloom::BuildAddPrefixList(SBPair* adds) {
  // Read add_prefix into memory and sort it.
  STATS_COUNTER("SB.HostSelectForBloomFilter", 1);
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

bool SafeBrowsingDatabaseBloom::RemoveSubs(
    SBPair* adds, std::vector<bool>* adds_removed, 
    HashCache* add_cache, HashCache* sub_cache, int* subs) {
  DCHECK(add_cache && sub_cache && subs);

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

  // Create a temporary sub full hash table, similar to the above prefix table.
  if (sqlite3_exec(db_, "CREATE TABLE sub_full_tmp ("
                   "chunk INTEGER,"
                   "add_chunk INTEGER,"
                   "prefix INTEGER,"
                   "full_hash BLOB)",
                   NULL, NULL, NULL) != SQLITE_OK) {
    return false;
  }

  SQLITE_UNIQUE_STATEMENT(
      sub_prefix_tmp,
      *statement_cache_,
      "INSERT INTO sub_prefix_tmp (chunk, add_chunk, prefix) VALUES (?,?,?)");
  if (!sub_prefix_tmp.is_valid()) {
    NOTREACHED();
    return false;
  }

  SBPair sub;
  int sub_count = 0;
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
      // Remove any GetHash results (full hashes) that match this sub, as well
      // as removing any full subs we may have received.
      ClearCachedEntry(sub.prefix, sub.chunk_id, add_cache);
      ClearCachedEntry(sub.prefix, sub.chunk_id, sub_cache);
    } else {
      // This sub_prefix entry did not match any add, so we keep it around.
      sub_prefix_tmp->bind_int(0, sub_chunk);
      sub_prefix_tmp->bind_int(1, sub.chunk_id);
      sub_prefix_tmp->bind_int(2, sub.prefix);
      rv = sub_prefix_tmp->step();
      if (rv == SQLITE_CORRUPT) {
        HandleCorruptDatabase();
        return false;
      }
      DCHECK(rv == SQLITE_DONE);
      sub_prefix_tmp->reset();
      ++sub_count;
    }
  }

  *subs = sub_count;
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

  // Now blow away add_prefix. We will write the new values out later.
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

  // Delete the old sub_full_hash table and rename the temp full hash table.
  SQLITE_UNIQUE_STATEMENT(del_full_sub, *statement_cache_,
                          "DROP TABLE sub_full_hash");
  if (!del_full_sub.is_valid()) {
    NOTREACHED();
    return false;
  }

  rv = del_full_sub->step();
  if (rv == SQLITE_CORRUPT) {
    HandleCorruptDatabase();
    return false;
  }
  DCHECK(rv == SQLITE_DONE);

  SQLITE_UNIQUE_STATEMENT(rename_full_sub, *statement_cache_,
                          "ALTER TABLE sub_full_tmp RENAME TO sub_full_hash");
  if (!rename_full_sub.is_valid()) {
    NOTREACHED();
    return false;
  }
  rv = rename_full_sub->step();
  if (rv == SQLITE_CORRUPT) {
    HandleCorruptDatabase();
    return false;
  }
  DCHECK(rv == SQLITE_DONE);

  // Blow away all the full adds. We will write the new values out later.
  SQLITE_UNIQUE_STATEMENT(del_full_add, *statement_cache_,
                          "DELETE FROM add_full_hash");
  if (!del_full_add.is_valid()) {
    NOTREACHED();
    return false;
  }
  rv = del_full_add->step();
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

void SafeBrowsingDatabaseBloom::WriteFullHashes(HashCache* hash_cache,
                                                bool is_add) {
  DCHECK(hash_cache);
  HashCache::iterator it = hash_cache->begin();
  for (; it != hash_cache->end(); ++it) {
    const HashList& entries = it->second;
    WriteFullHashList(entries, is_add);
  }
}

void SafeBrowsingDatabaseBloom::WriteFullHashList(const HashList& hash_list,
                                                  bool is_add) {
  HashList::const_iterator lit = hash_list.begin();
  for (; lit != hash_list.end(); ++lit) {
    const HashCacheEntry& entry = *lit;
    SBPrefix prefix;
    memcpy(&prefix, entry.full_hash.full_hash, sizeof(SBPrefix));
    if (is_add) {
      if (add_del_cache_.find(entry.add_chunk_id) == add_del_cache_.end()) {
        InsertAddFullHash(prefix, entry.add_chunk_id,
                          entry.received, entry.full_hash);
      }
    } else {
      if (sub_del_cache_.find(entry.sub_chunk_id) == sub_del_cache_.end()) {
        InsertSubFullHash(prefix, entry.sub_chunk_id,
                          entry.add_chunk_id, entry.full_hash, true);
      }
    }
  }
}

bool SafeBrowsingDatabaseBloom::BuildAddFullHashCache(HashCache* add_cache) {
  add_cache->clear();

  // Read all full add entries to the cache.
  SQLITE_UNIQUE_STATEMENT(
      full_add_entry,
      *statement_cache_,
      "SELECT chunk, prefix, receive_time, full_hash FROM add_full_hash");
  if (!full_add_entry.is_valid()) {
    NOTREACHED();
    return false;
  }

  int rv;
  while (true) {
    rv = full_add_entry->step();
    if (rv != SQLITE_ROW) {
      if (rv == SQLITE_CORRUPT) {
        HandleCorruptDatabase();
        return false;
      }
      break;
    }
    HashCacheEntry entry;
    entry.add_chunk_id = full_add_entry->column_int(0);
    if (add_del_cache_.find(entry.add_chunk_id) != add_del_cache_.end())
      continue;  // This entry's chunk was deleted so we skip it.
    SBPrefix prefix = full_add_entry->column_int(1);
    entry.received = base::Time::FromTimeT(full_add_entry->column_int64(2));
    int chunk, list_id;
    DecodeChunkId(entry.add_chunk_id, &chunk, &list_id);
    entry.list_id = list_id;
    ReadFullHash(full_add_entry, 3, &entry.full_hash);
    HashList& entries = (*add_cache)[prefix];
    entries.push_back(entry);
  }

  // Clear the full add table.
  SQLITE_UNIQUE_STATEMENT(full_add_drop, *statement_cache_,
                          "DELETE FROM add_full_hash");
  if (!full_add_drop.is_valid()) {
    NOTREACHED();
    return false;
  }
  rv = full_add_drop->step();
  if (rv == SQLITE_CORRUPT) {
    HandleCorruptDatabase();
    return false;
  }
  DCHECK(rv == SQLITE_DONE);

  return true;
}

bool SafeBrowsingDatabaseBloom::BuildSubFullHashCache(HashCache* sub_cache) {
  sub_cache->clear();

  // Read all full sub entries to the cache.
  SQLITE_UNIQUE_STATEMENT(
      full_sub_entry,
      *statement_cache_,
      "SELECT chunk, add_chunk, prefix, full_hash FROM sub_full_hash");
  if (!full_sub_entry.is_valid()) {
    NOTREACHED();
    return false;
  }

  int rv;
  while (true) {
    rv = full_sub_entry->step();
    if (rv != SQLITE_ROW) {
      if (rv == SQLITE_CORRUPT) {
        HandleCorruptDatabase();
        return false;
      }
      break;
    }
    HashCacheEntry entry;
    entry.sub_chunk_id = full_sub_entry->column_int(0);
    if (sub_del_cache_.find(entry.sub_chunk_id) != sub_del_cache_.end())
      continue;  // This entry's chunk was deleted so we skip it.
    entry.add_chunk_id = full_sub_entry->column_int(1);
    SBPrefix prefix = full_sub_entry->column_int(2);
    int chunk, list_id;
    DecodeChunkId(entry.add_chunk_id, &chunk, &list_id);
    entry.list_id = list_id;
    ReadFullHash(full_sub_entry, 3, &entry.full_hash);
    HashList& entries = (*sub_cache)[prefix];
    entries.push_back(entry);
  }

  // Clear the full sub table.
  SQLITE_UNIQUE_STATEMENT(full_sub_drop, *statement_cache_,
                          "DELETE FROM sub_full_hash");
  if (!full_sub_drop.is_valid()) {
    NOTREACHED();
    return false;
  }
  rv = full_sub_drop->step();
  if (rv == SQLITE_CORRUPT) {
    HandleCorruptDatabase();
    return false;
  }
  DCHECK(rv == SQLITE_DONE);

  return true;
}

// TODO(erikkay): should we call WaitAfterResume() inside any of the loops here?
// This is a pretty fast operation and it would be nice to let it finish.
void SafeBrowsingDatabaseBloom::BuildBloomFilter() {
#if defined(OS_WIN)
  // For measuring the amount of IO during the bloom filter build.
  IoCounters io_before, io_after;
  base::ProcessHandle handle = base::Process::Current().handle();
  scoped_ptr<base::ProcessMetrics> metric;
  metric.reset(base::ProcessMetrics::CreateProcessMetrics(handle));
  metric->GetIOCounters(&io_before);
#endif

  Time before = Time::Now();

  // Get all the pending GetHash results and write them to disk.
  HashList pending_hashes;
  {
    AutoLock lock(lookup_lock_);
    pending_hashes.swap(pending_full_hashes_);
  }
  WriteFullHashList(pending_hashes, true);

  add_count_ = GetAddPrefixCount();
  if (add_count_ == 0) {
    bloom_filter_ = NULL;
    return;
  }

  scoped_array<SBPair> adds_array(new SBPair[add_count_]);
  SBPair* adds = adds_array.get();

  if (!BuildAddPrefixList(adds))
    return;

  // Build the full add cache, which includes full hash updates and GetHash
  // results. Subs may remove some of these entries.
  scoped_ptr<HashCache> add_cache(new HashCache);
  if (!BuildAddFullHashCache(add_cache.get()))
    return;

  scoped_ptr<HashCache> sub_cache(new HashCache);
  if (!BuildSubFullHashCache(sub_cache.get()))
    return;

  // Used to track which adds have been subbed out. The vector<bool> is actually
  // a bitvector so the size is as small as we can get.
  std::vector<bool> adds_removed;
  adds_removed.resize(add_count_, false);

  // Flag any add as removed if there is a matching sub.
  int subs = 0;
  if (!RemoveSubs(adds, &adds_removed, add_cache.get(), sub_cache.get(), &subs))
    return;

  // Prepare the database for writing out our remaining add and sub prefixes.
  if (!UpdateTables())
    return;

  // Write out the remaining add prefixes to the filter and database.
  int new_count;
  BloomFilter* filter;
  if (!WritePrefixes(adds, adds_removed, &new_count, &filter))
    return;

  // Write out the remaining full hash adds and subs to the database.
  WriteFullHashes(add_cache.get(), true);
  WriteFullHashes(sub_cache.get(), false);

  // Save the chunk numbers we've received to the database for reporting in
  // future update requests.
  if (!WriteChunkNumbers())
    return;

  // Commit all the changes to the database.
  int rv = insert_transaction_->Commit();
  if (rv != SQLITE_OK) {
    NOTREACHED() << "SafeBrowsing update transaction failed to commit.";
    UMA_HISTOGRAM_COUNTS(L"SB2.FailedUpdate", 1);
    return;
  }

  // Swap in the newly built filter and cache. If there were any matching subs,
  // the size (add_count_) will be smaller.
  {
    AutoLock lock(lookup_lock_);
    add_count_ = new_count;
    bloom_filter_ = filter;
    hash_cache_.swap(add_cache);
  }

  TimeDelta bloom_gen = Time::Now() - before;

  // Persist the bloom filter to disk.
  WriteBloomFilter();

  // Gather statistics.
#if defined(OS_WIN)
  metric->GetIOCounters(&io_after);
  UMA_HISTOGRAM_COUNTS(L"SB2.BuildReadBytes",
                       static_cast<int>(io_after.ReadTransferCount -
                                        io_before.ReadTransferCount));
  UMA_HISTOGRAM_COUNTS(L"SB2.BuildWriteBytes",
                       static_cast<int>(io_after.WriteTransferCount -
                                        io_before.WriteTransferCount));
  UMA_HISTOGRAM_COUNTS(L"SB2.BuildReadOperations",
                       static_cast<int>(io_after.ReadOperationCount -
                                        io_before.ReadOperationCount));
  UMA_HISTOGRAM_COUNTS(L"SB2.BuildWriteOperations",
                       static_cast<int>(io_after.WriteOperationCount -
                                        io_before.WriteOperationCount));
#endif
  SB_DLOG(INFO) << "SafeBrowsingDatabaseImpl built bloom filter in "
                << bloom_gen.InMilliseconds()
                << " ms total.  prefix count: "<< add_count_;
  UMA_HISTOGRAM_LONG_TIMES(L"SB2.BuildFilter", bloom_gen);
  UMA_HISTOGRAM_COUNTS(L"SB2.AddPrefixes", add_count_);
  UMA_HISTOGRAM_COUNTS(L"SB2.SubPrefixes", subs);
  int64 size_64;
  if (file_util::GetFileSize(filename_, &size_64))
    UMA_HISTOGRAM_COUNTS(L"SB2.DatabaseBytes", static_cast<int>(size_64));
}

void SafeBrowsingDatabaseBloom::GetCachedFullHashes(
    const std::vector<SBPrefix>* prefix_hits,
    std::vector<SBFullHashResult>* full_hits,
    Time last_update) {
  DCHECK(prefix_hits && full_hits);

  Time max_age = Time::Now() - TimeDelta::FromMinutes(kMaxStalenessMinutes);

  for (std::vector<SBPrefix>::const_iterator it = prefix_hits->begin();
       it != prefix_hits->end(); ++it) {
    HashCache::iterator hit = hash_cache_->find(*it);
    if (hit != hash_cache_->end()) {
      HashList& entries = hit->second;
      HashList::iterator eit = entries.begin();
      while (eit != entries.end()) {
        // An entry is valid if we've received an update in the past 45 minutes,
        // or if this particular GetHash was received in the past 45 minutes.
        // If an entry is does not meet the time criteria above, we are not
        // allowed to use it since it might have become stale. We keep it
        // around, though, and may be able to use it in the future once we
        // receive the next update (that doesn't sub it).
        if (max_age < last_update || eit->received > max_age) {
          SBFullHashResult full_hash;
          memcpy(&full_hash.hash.full_hash,
                 &eit->full_hash.full_hash,
                 sizeof(SBFullHash));
          full_hash.list_name = safe_browsing_util::GetListName(eit->list_id);
          full_hash.add_chunk_id = eit->add_chunk_id;
          full_hits->push_back(full_hash);
        }
        ++eit;
      }

      if (entries.empty())
        hash_cache_->erase(hit);
    }
  }
}

void SafeBrowsingDatabaseBloom::CacheHashResults(
    const std::vector<SBPrefix>& prefixes,
    const std::vector<SBFullHashResult>& full_hits) {
  AutoLock lock(lookup_lock_);

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
    memcpy(&prefix, &it->hash.full_hash, sizeof(SBPrefix));
    HashList& entries = (*hash_cache_)[prefix];
    HashCacheEntry entry;
    entry.received = now;
    entry.list_id = safe_browsing_util::GetListId(it->list_name);
    entry.add_chunk_id = EncodeChunkId(it->add_chunk_id, entry.list_id);
    memcpy(&entry.full_hash, &it->hash.full_hash, sizeof(SBFullHash));
    entries.push_back(entry);

    // Also push a copy to the pending write queue.
    pending_full_hashes_.push_back(entry);
  }
}

bool SafeBrowsingDatabaseBloom::ClearCachedEntry(SBPrefix prefix,
                                                 int add_chunk,
                                                 HashCache* hash_cache) {
  bool match = false;
  HashCache::iterator it = hash_cache->find(prefix);
  if (it == hash_cache->end())
    return match;

  HashList& entries = it->second;
  HashList::iterator lit = entries.begin();
  while (lit != entries.end()) {
    HashCacheEntry& entry = *lit;
    if (entry.add_chunk_id == add_chunk) {
      lit = entries.erase(lit);
      match = true;
      continue;
    }
    ++lit;
  }

  if (entries.empty())
    hash_cache->erase(it);

  return match;
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
