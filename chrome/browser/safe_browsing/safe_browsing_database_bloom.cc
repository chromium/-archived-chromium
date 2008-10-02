// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/safe_browsing_database_bloom.h"

#include "base/file_util.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/sha2.h"
#include "base/string_util.h"
#include "chrome/browser/safe_browsing/bloom_filter.h"
#include "chrome/browser/safe_browsing/chunk_range.h"
#include "chrome/common/sqlite_compiled_statement.h"
#include "chrome/common/sqlite_utils.h"
#include "googleurl/src/gurl.h"

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
      transaction_count_(0),
      chunk_inserted_callback_(NULL),
#pragma warning(suppress: 4355)  // can use this
      reset_factory_(this),
#pragma warning(suppress: 4355)  // can use this
      resume_factory_(this),
      did_resume_(false) {
}

SafeBrowsingDatabaseBloom::~SafeBrowsingDatabaseBloom() {
  Close();
}

bool SafeBrowsingDatabaseBloom::Init(const std::wstring& filename,
                                    Callback0::Type* chunk_inserted_callback) {
  DCHECK(!init_ && filename_.empty());

  filename_ = filename;
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

  bloom_filter_filename_ = BloomFilterFilename(filename_);

  if (load_filter) {
    LoadBloomFilter();
  } else {
    bloom_filter_.reset(
        new BloomFilter(kBloomFilterMinSize * kBloomFilterSizeRatio));
  }

  init_ = true;
  chunk_inserted_callback_ = chunk_inserted_callback;
  return true;
}

bool SafeBrowsingDatabaseBloom::Open() {
  if (sqlite3_open(WideToUTF8(filename_).c_str(), &db_) != SQLITE_OK)
    return false;

  // Run the database in exclusive mode. Nobody else should be accessing the
  // database while we're running, and this will give somewhat improved perf.
  sqlite3_exec(db_, "PRAGMA locking_mode=EXCLUSIVE", NULL, NULL, NULL);

  statement_cache_.reset(new SqliteStatementCache(db_));

  CreateChunkCaches();

  return true;
}

bool SafeBrowsingDatabaseBloom::Close() {
  if (!db_)
    return true;

  if (!pending_add_del_.empty()) {
    while (!pending_add_del_.empty())
      pending_add_del_.pop();

    EndTransaction();
  }

  while (!pending_chunks_.empty()) {
    std::deque<SBChunk>* chunks = pending_chunks_.front();
    safe_browsing_util::FreeChunks(chunks);
    delete chunks;
    pending_chunks_.pop();
    EndTransaction();
  }

  statement_cache_.reset();  // Must free statements before closing DB.
  transaction_.reset();
  bool result = sqlite3_close(db_) == SQLITE_OK;
  db_ = NULL;
  return result;
}

bool SafeBrowsingDatabaseBloom::CreateTables() {
  SQLTransaction transaction(db_);
  transaction.Begin();

  if (sqlite3_exec(db_, "CREATE TABLE add_prefix ("
      "chunk INTEGER,"
      "prefix INTEGER)",
      NULL, NULL, NULL) != SQLITE_OK) {
    return false;
  }

  if (sqlite3_exec(db_, "CREATE TABLE sub_prefix ("
                   "chunk INTEGER,"
                   "add_chunk INTEGER,"
                   "prefix INTEGER)",
                   NULL, NULL, NULL) != SQLITE_OK) {
    return false;
  }

  if (sqlite3_exec(db_, "CREATE TABLE full_prefix ("
                   "chunk INTEGER,"
                   "prefix INTEGER,"
                   "full_prefix BLOB)",
                   NULL, NULL, NULL) != SQLITE_OK) {
    return false;
  }
  sqlite3_exec(db_, "CREATE INDEX full_prefix_chunk ON full_prefix(chunk)",
               NULL, NULL, NULL);
  sqlite3_exec(db_, "CREATE INDEX full_prefix_prefix ON full_prefix(prefix)",
               NULL, NULL, NULL);

  if (sqlite3_exec(db_, "CREATE TABLE list_names ("
                   "id INTEGER PRIMARY KEY AUTOINCREMENT,"
                   "name TEXT)",
                   NULL, NULL, NULL) != SQLITE_OK) {
    return false;
  }

  if (sqlite3_exec(db_, "CREATE TABLE add_chunk ("
                   "chunk INTEGER PRIMARY KEY)",
                   NULL, NULL, NULL) != SQLITE_OK) {
    return false;
  }

  if (sqlite3_exec(db_, "CREATE TABLE sub_chunk ("
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

// The SafeBrowsing service assumes this operation is synchronous.
bool SafeBrowsingDatabaseBloom::ResetDatabase() {
  hash_cache_.clear();
  prefix_miss_cache_.clear();

  bool rv = Close();
  DCHECK(rv);

  if (!file_util::Delete(filename_, false)) {
    NOTREACHED();
    return false;
  }

  bloom_filter_.reset(
      new BloomFilter(kBloomFilterMinSize * kBloomFilterSizeRatio));
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

bool SafeBrowsingDatabaseBloom::ContainsUrl(
    const GURL& url,
    std::string* matching_list,
    std::vector<SBPrefix>* prefix_hits,
    std::vector<SBFullHashResult>* full_hits,
    Time last_update) {

  std::vector<std::string> hosts;
  if (url.HostIsIPAddress()) {
    hosts.push_back(url.host());
  } else {
    safe_browsing_util::GenerateHostsToCheck(url, &hosts);
    if (hosts.size() == 0)
      return false; // things like about:blank
  }
  std::vector<std::string> paths;
  safe_browsing_util::GeneratePathsToCheck(url, &paths);

  // TODO(erikkay): this may wind up being too many hashes on a complex page
  // TODO(erikkay): not filling in matching_list - is that OK?
  // TODO(erikkay): handle full_hits
  for (size_t i = 0; i < hosts.size(); ++i) {
    for (size_t j = 0; j < paths.size(); ++j) {
      SBFullHash full_hash;
      // TODO(erikkay): maybe we should only do the first 32 bits initially,
      // and then fall back to the full hash if there's a hit.
      base::SHA256HashString(hosts[i] + paths[j], &full_hash, 
                             sizeof(SBFullHash));
      SBPrefix prefix;
      memcpy(&prefix, &full_hash, sizeof(SBPrefix));
      if (bloom_filter_->Exists(prefix))
        prefix_hits->push_back(prefix);
    }
  }
  return prefix_hits->size() > 0;
}

bool SafeBrowsingDatabaseBloom::NeedToCheckUrl(const GURL& url) {
  // Since everything is in the bloom filter, doing anything here would wind
  // up just duplicating work that would happen in ContainsURL.
  // It's possible that we may want to add a hostkey-based first-level cache
  // on the front of this to minimize hash generation, but we'll need to do
  // some measurements to verify that.
  return true;
}

void SafeBrowsingDatabaseBloom::ProcessPendingWork() {
  BeginTransaction();
  prefix_miss_cache_.clear();
  ProcessChunks();
  ProcessAddDel();
  EndTransaction();
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
  //DeleteBloomFilter();

  int list_id = GetListID(list_name);
  std::deque<SBChunk>::iterator i = chunks->begin();
  for (; i != chunks->end(); ++i) {
    SBChunk& chunk = (*i);
    std::deque<SBChunkHost>::iterator j = chunk.hosts.begin();
    for (; j != chunk.hosts.end(); ++j) {
      j->entry->set_list_id(list_id);
      if (j->entry->IsAdd())
        j->entry->set_chunk_id(chunk.chunk_number);
    }
  }

  pending_chunks_.push(chunks);

  ProcessPendingWork();
}

void SafeBrowsingDatabaseBloom::UpdateFinished() {
  BuildBloomFilter();
}

void SafeBrowsingDatabaseBloom::ProcessChunks() {
  if (pending_chunks_.empty())
    return;

  while (!pending_chunks_.empty()) {
    std::deque<SBChunk>* chunks = pending_chunks_.front();
    // The entries in one chunk are all either adds or subs.
    if (chunks->front().hosts.front().entry->IsAdd()) {
      ProcessAddChunks(chunks);
    } else {
      ProcessSubChunks(chunks);
    }

    delete chunks;
    pending_chunks_.pop();
  }

  if (chunk_inserted_callback_)
    chunk_inserted_callback_->Run();
}

void SafeBrowsingDatabaseBloom::ProcessAddChunks(std::deque<SBChunk>* chunks) {
  while (!chunks->empty()) {
    SBChunk& chunk = chunks->front();
    int list_id = chunk.hosts.front().entry->list_id();
    int chunk_id = chunk.chunk_number;

    // The server can give us a chunk that we already have because it's part of
    // a range.  Don't add it again.
    if (!ChunkExists(list_id, ADD_CHUNK, chunk_id)) {
      InsertChunk(list_id, ADD_CHUNK, chunk_id);
      while (!chunk.hosts.empty()) {
        WaitAfterResume();
        // Read the existing record for this host, if it exists.
        SBPrefix host = chunk.hosts.front().host;
        SBEntry* entry = chunk.hosts.front().entry;

        AddEntry(host, entry);

        entry->Destroy();
        chunk.hosts.pop_front();
      }
      int encoded = EncodedChunkId(chunk_id, list_id);
      add_chunk_cache_.insert(encoded);
    }

    chunks->pop_front();
  }
}

void SafeBrowsingDatabaseBloom::AddEntry(SBPrefix host, SBEntry* entry) {
  STATS_COUNTER(L"SB.HostInsert", 1);
  if (entry->type() == SBEntry::ADD_FULL_HASH) {
    // TODO(erikkay)
    return;
  }
  int encoded = EncodedChunkId(entry->chunk_id(), entry->list_id());
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

void SafeBrowsingDatabaseBloom::AddSub(
    int chunk_id, SBPrefix host, SBEntry* entry) {
  STATS_COUNTER(L"SB.HostDelete", 1);
  if (entry->type() == SBEntry::SUB_FULL_HASH) {
    // TODO(erikkay)
    return;
  }

  int encoded = EncodedChunkId(chunk_id, entry->list_id());
  int encoded_add = EncodedChunkId(entry->chunk_id(), entry->list_id());
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

// Encode the list id in the lower bit of the chunk.
static inline int EncodeChunkId(int chunk, int list_id) {
  list_id--;
  DCHECK(list_id == 0 || list_id == 1);
  chunk = chunk << 1;
  chunk |= list_id;
  return chunk;
}

// Split an encoded chunk id and return the original chunk id and list id.
static inline void DecodeChunkId(int encoded, int* chunk, int* list_id) {
  *list_id = 1 + (encoded & 0x1);
  *chunk = encoded >> 1;
}

// TODO(erikkay) - this is too slow
void SafeBrowsingDatabaseBloom::CreateChunkCaches() {
  SQLITE_UNIQUE_STATEMENT(adds, *statement_cache_,
      "SELECT distinct chunk FROM add_prefix");
  if (!adds.is_valid()) {
    NOTREACHED();
    return;
  }
  int rv = adds->step();
  while (rv == SQLITE_ROW) {
    int chunk = adds->column_int(0);
    add_chunk_cache_.insert(chunk);
    rv = adds->step();
  }
  SQLITE_UNIQUE_STATEMENT(subs, *statement_cache_,
      "SELECT distinct chunk FROM sub_prefix");
  if (!subs.is_valid()) {
    NOTREACHED();
    return;
  }
  rv = subs->step();
  while (rv == SQLITE_ROW) {
    int chunk = subs->column_int(0);
    sub_chunk_cache_.insert(chunk);
    rv = subs->step();
  }

  SQLITE_UNIQUE_STATEMENT(count, *statement_cache_,
      "SELECT count(*) FROM add_prefix");
  if (!count.is_valid()) {
    NOTREACHED();
    return;
  }
  rv = count->step();
  if (rv == SQLITE_ROW)
    add_count_ = count->column_int(0);
  else if (rv == SQLITE_CORRUPT)
    HandleCorruptDatabase();
}

void SafeBrowsingDatabaseBloom::ProcessSubChunks(std::deque<SBChunk>* chunks) {
  while (!chunks->empty()) {
    SBChunk& chunk = chunks->front();
    int list_id = chunk.hosts.front().entry->list_id();
    int chunk_id = chunk.chunk_number;

    if (!ChunkExists(list_id, SUB_CHUNK, chunk_id)) {
      InsertChunk(list_id, SUB_CHUNK, chunk_id);
      while (!chunk.hosts.empty()) {
        WaitAfterResume();

        SBPrefix host = chunk.hosts.front().host;
        SBEntry* entry = chunk.hosts.front().entry;
        AddSub(chunk_id, host, entry);

        entry->Destroy();
        chunk.hosts.pop_front();
      }

      int encoded = EncodedChunkId(chunk_id, list_id);
      sub_chunk_cache_.insert(encoded);
    }

    chunks->pop_front();
  }
}

void SafeBrowsingDatabaseBloom::DeleteChunks(
    std::vector<SBChunkDelete>* chunk_deletes) {
  BeginTransaction();
  bool pending_add_del_were_empty = pending_add_del_.empty();

  for (size_t i = 0; i < chunk_deletes->size(); ++i) {
    const SBChunkDelete& chunk = (*chunk_deletes)[i];
    std::vector<int> chunk_numbers;
    RangesToChunks(chunk.chunk_del, &chunk_numbers);
    for (size_t del = 0; del < chunk_numbers.size(); ++del) {
      if (chunk.is_sub_del) {
        SubDel(chunk.list_name, chunk_numbers[del]);
      } else {
        AddDel(chunk.list_name, chunk_numbers[del]);
      }
    }
  }

  if (pending_add_del_were_empty && !pending_add_del_.empty()) {
    ProcessPendingWork();
  }

  delete chunk_deletes;
  EndTransaction();
}

void SafeBrowsingDatabaseBloom::AddDel(const std::string& list_name,
                                       int add_chunk_id) {
  int list_id = GetListID(list_name);
  AddDel(list_id, add_chunk_id);
}

void SafeBrowsingDatabaseBloom::AddDel(int list_id,
                                       int add_chunk_id) {
  STATS_COUNTER(L"SB.ChunkDelete", 1);
  // Find all the prefixes that came from the given add_chunk_id.
  SQLITE_UNIQUE_STATEMENT(statement, *statement_cache_,
      "DELETE FROM add_prefix WHERE chunk=?");
  if (!statement.is_valid()) {
    NOTREACHED();
    return;
  }

  int encoded = EncodedChunkId(add_chunk_id, list_id);
  statement->bind_int(0, encoded);
  int rv = statement->step();
  if (rv == SQLITE_CORRUPT) {
    HandleCorruptDatabase();
  } else {
    DCHECK(rv == SQLITE_DONE);
  }
  add_chunk_cache_.erase(encoded);
}

void SafeBrowsingDatabaseBloom::SubDel(const std::string& list_name,
                                       int sub_chunk_id) {
  int list_id = GetListID(list_name);
  SubDel(list_id, sub_chunk_id);
}

void SafeBrowsingDatabaseBloom::SubDel(int list_id,
                                       int sub_chunk_id) {
  STATS_COUNTER(L"SB.ChunkDelete", 1);
  // Find all the prefixes that came from the given add_chunk_id.
  SQLITE_UNIQUE_STATEMENT(statement, *statement_cache_,
      "DELETE FROM sub_prefix WHERE chunk=?");
  if (!statement.is_valid()) {
    NOTREACHED();
    return;
  }

  int encoded = EncodedChunkId(sub_chunk_id, list_id);
  statement->bind_int(0, encoded);
  int rv = statement->step();
  if (rv == SQLITE_CORRUPT) {
    HandleCorruptDatabase();
  } else {
    DCHECK(rv == SQLITE_DONE);
  }
  sub_chunk_cache_.erase(encoded);
}

void SafeBrowsingDatabaseBloom::ProcessAddDel() {
  if (pending_add_del_.empty())
    return;

  while (!pending_add_del_.empty()) {
    AddDelWork& add_del_work = pending_add_del_.front();
    ClearCachedHashesForChunk(add_del_work.list_id, add_del_work.add_chunk_id);
    AddDel(add_del_work.list_id, add_del_work.add_chunk_id);
    pending_add_del_.pop();
  }
}

bool SafeBrowsingDatabaseBloom::ChunkExists(int list_id,
                                            ChunkType type,
                                            int chunk_id) {
  STATS_COUNTER(L"SB.ChunkSelect", 1);
  int encoded = EncodedChunkId(chunk_id, list_id);
  bool ret;
  if (type == ADD_CHUNK)
    ret = add_chunk_cache_.count(encoded) > 0;
  else
    ret = sub_chunk_cache_.count(encoded) > 0;

  return ret;
}

void SafeBrowsingDatabaseBloom::InsertChunk(int list_id,
                                            ChunkType type,
                                            int chunk_id) {
  // TODO(erikkay): insert into the correct chunk table
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

typedef struct {
  int chunk_id;
  SBPrefix prefix;
} SBPair;

static int pair_compare(const void* arg1, const void* arg2) {
  const SBPair* p1 = reinterpret_cast<const SBPair*>(arg1);
  const SBPair* p2 = reinterpret_cast<const SBPair*>(arg2);
  int delta = p1->chunk_id - p2->chunk_id;
  if (delta == 0)
    delta = p1->prefix - p2->prefix;
  return delta;
}

// TODO(erikkay): should we call WaitAfterResume() inside any of the loops here?
// This is a pretty fast operation and it would be nice to let it finish.
void SafeBrowsingDatabaseBloom::BuildBloomFilter() {
  Time before = Time::Now();

  scoped_array<SBPair> adds_array(new SBPair[add_count_]);
  SBPair* adds = adds_array.get();

  // Read add_prefix into memory and sort it.
  STATS_COUNTER(L"SB.HostSelectForBloomFilter", 1);
  SQLITE_UNIQUE_STATEMENT(add_prefix, *statement_cache_,
      "SELECT chunk, prefix FROM add_prefix");
  if (!add_prefix.is_valid()) {
    NOTREACHED();
    return;
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
  qsort(adds, add_count_, sizeof(SBPair), pair_compare);

  // Read through sub_prefix and zero out add_prefix entries that match.
  SQLITE_UNIQUE_STATEMENT(sub_prefix, *statement_cache_,
      "SELECT add_chunk, prefix FROM sub_prefix");
  if (!sub_prefix.is_valid()) {
    NOTREACHED();
    return;
  }
  SBPair sub;
  while (true) {
    int rv = sub_prefix->step();
    if (rv != SQLITE_ROW) {
      if (rv == SQLITE_CORRUPT)
        HandleCorruptDatabase();
      break;
    }
    sub.chunk_id = sub_prefix->column_int(0);
    sub.prefix = sub_prefix->column_int(1);
    void* match = bsearch(&sub, adds, add_count_, sizeof(SBPair), pair_compare);
    if (match) {
      SBPair* subbed = reinterpret_cast<SBPair*>(match);
      // A chunk_id of 0 is invalid, so we use that to mark this prefix as
      // having been subbed.
      subbed->chunk_id = 0;
    }
  }

  // Now blow away add_prefix and re-write it from our in-memory data,
  // building the new bloom filter as we go.
  BeginTransaction();
  SQLITE_UNIQUE_STATEMENT(del, *statement_cache_, "DELETE FROM add_prefix");
  if (!del.is_valid()) {
    NOTREACHED();
    return;
  }
  int rv = del->step();
  if (rv == SQLITE_CORRUPT) {
    HandleCorruptDatabase();
    return;
  }
  DCHECK(rv == SQLITE_DONE);

  SQLITE_UNIQUE_STATEMENT(insert, *statement_cache_,
      "INSERT INTO add_prefix VALUES(?,?)");
  if (!insert.is_valid()) {
    NOTREACHED();
    return;
  }
  int number_of_keys = std::max(add_count_, kBloomFilterMinSize);
  int filter_size = number_of_keys * kBloomFilterSizeRatio;
  BloomFilter* filter = new BloomFilter(filter_size);
  add = adds;
  int new_count = 0;
  while (add - adds < add_count_) {
    if (add->chunk_id != 0) {
      filter->Insert(add->prefix);
      insert->bind_int(0, add->prefix);
      insert->bind_int(1, add->chunk_id);
      rv = insert->step();
      if (rv == SQLITE_CORRUPT) {
        HandleCorruptDatabase();
        return;
      }
      DCHECK(rv == SQLITE_DONE);
      insert->reset();
      new_count++;
    }
    add++;
  }
  bloom_filter_.reset(filter);
  // If there were any matching subs, the size will be smaller.
  add_count_ = new_count;
  EndTransaction();

  TimeDelta bloom_gen = Time::Now() - before;
  SB_DLOG(INFO) << "SafeBrowsingDatabaseImpl built bloom filter in " <<
      bloom_gen.InMilliseconds() << " ms total.  prefix count: " <<
      add_count_;
  UMA_HISTOGRAM_LONG_TIMES(L"SB.BuildBloom", bloom_gen);

  WriteBloomFilter();
}

void SafeBrowsingDatabaseBloom::BeginTransaction() {
  transaction_count_++;
  if (transaction_.get() == NULL) {
    transaction_.reset(new SQLTransaction(db_));
    if (transaction_->Begin() != SQLITE_OK) {
      DCHECK(false) << "Safe browsing database couldn't start transaction";
      transaction_.reset();
    }
  }
}

void SafeBrowsingDatabaseBloom::EndTransaction() {
  if (--transaction_count_ == 0) {
    if (transaction_.get() != NULL) {
      STATS_COUNTER(L"SB.TransactionCommit", 1);
      transaction_->Commit();
      transaction_.reset();
    }
  }
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
      it = hash_cache_.erase(it);
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
    Sleep(kOnResumeHoldupMs);
    did_resume_ = false;
  }
}

// This database is always synchronous since we don't need to worry about
// blocking any incoming reads.
void SafeBrowsingDatabaseBloom::SetSynchronous() {
}
