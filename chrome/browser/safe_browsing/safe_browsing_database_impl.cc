// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/safe_browsing_database_impl.h"

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
static const int kDatabaseVersion = 4;

// Don't want to create too small of a bloom filter initially while we're
// downloading the data and then keep having to rebuild it.
static const int kBloomFilterMinSize = 250000;

// How many bits to use per item.  See the design doc for more information.
static const int kBloomFilterSizeRatio = 13;

// The minimum number of reads/misses before we will consider rebuilding the
// bloom filter.  This is needed because we don't want a few misses after
// starting the browser to skew the percentage.
// TODO(jabdelmalek): report to UMA how often we rebuild.
static const int kBloomFilterMinReadsToCheckFP = 200;

// The percentage of hit rate in the bloom filter when we regenerate it.
static const double kBloomFilterMaxFPRate = 5.0;

// When we awake from a low power state, we try to avoid doing expensive disk
// operations for a few minutes to let the system page itself in and settle
// down.
static const int kOnResumeHoldupMs = 5 * 60 * 1000;  // 5 minutes.

// When doing any database operations that can take a long time, we do it in
// small chunks up to this amount.  Once this much time passes, we sleep for
// the same amount and continue.  This avoids blocking the thread so that if
// we get a bloom filter hit, we don't block the network request.
static const int kMaxThreadHoldupMs = 100;

// How long to wait after updating the database to write the bloom filter.
static const int kBloomFilterWriteDelayMs = (60 * 1000);

// The maximum staleness for a cached entry.
static const int kMaxStalenessMinutes = 45;

// Implementation --------------------------------------------------------------

SafeBrowsingDatabaseImpl::SafeBrowsingDatabaseImpl()
    : db_(NULL),
      transaction_count_(0),
      init_(false),
      asynchronous_(true),
      ALLOW_THIS_IN_INITIALIZER_LIST(process_factory_(this)),
      ALLOW_THIS_IN_INITIALIZER_LIST(bloom_read_factory_(this)),
      ALLOW_THIS_IN_INITIALIZER_LIST(bloom_write_factory_(this)),
      ALLOW_THIS_IN_INITIALIZER_LIST(reset_factory_(this)),
      ALLOW_THIS_IN_INITIALIZER_LIST(resume_factory_(this)),
      disk_delay_(kMaxThreadHoldupMs) {
}

SafeBrowsingDatabaseImpl::~SafeBrowsingDatabaseImpl() {
  Close();
}

bool SafeBrowsingDatabaseImpl::Init(const FilePath& filename,
                                    Callback0::Type* chunk_inserted_callback) {
  DCHECK(!init_ && filename_.empty());

  filename_ = filename;
  if (!Open())
    return false;

  bool load_filter = false;
  if (!DoesSqliteTableExist(db_, "hosts")) {
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
    bloom_filter_ =
        new BloomFilter(kBloomFilterMinSize * kBloomFilterSizeRatio);
  }

  init_ = true;
  chunk_inserted_callback_.reset(chunk_inserted_callback);

  return true;
}

bool SafeBrowsingDatabaseImpl::Open() {
  if (OpenSqliteDb(filename_, &db_) != SQLITE_OK)
    return false;

  // Run the database in exclusive mode. Nobody else should be accessing the
  // database while we're running, and this will give somewhat improved perf.
  sqlite3_exec(db_, "PRAGMA locking_mode=EXCLUSIVE", NULL, NULL, NULL);

  statement_cache_.reset(new SqliteStatementCache(db_));
  bloom_filter_read_count_= 0;
  bloom_filter_fp_count_ = 0;
  bloom_filter_building_ = false;

  process_factory_.RevokeAll();
  bloom_read_factory_.RevokeAll();
  bloom_write_factory_.RevokeAll();

  hash_cache_.reset(new HashCache);

  return true;
}

bool SafeBrowsingDatabaseImpl::Close() {
  if (!db_)
    return true;

  process_factory_.RevokeAll();
  bloom_read_factory_.RevokeAll();
  bloom_write_factory_.RevokeAll();

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

bool SafeBrowsingDatabaseImpl::CreateTables() {
  SQLTransaction transaction(db_);
  transaction.Begin();

  // We use an autoincrement integer as the primary key to allow full table
  // scans to be quick.  Otherwise if we used host, then we'd have to jump
  // all over the table when doing a full table scan to generate the bloom
  // filter and that's an order of magnitude slower.  By marking host as
  // unique, an index is created automatically.
  if (sqlite3_exec(db_, "CREATE TABLE hosts ("
      "id INTEGER PRIMARY KEY AUTOINCREMENT,"
      "host INTEGER UNIQUE,"
      "entries BLOB)",
      NULL, NULL, NULL) != SQLITE_OK) {
    return false;
  }

  if (sqlite3_exec(db_, "CREATE TABLE chunks ("
      "list_id INTEGER,"
      "chunk_type INTEGER,"
      "chunk_id INTEGER,"
      "hostkeys TEXT)",
      NULL, NULL, NULL) != SQLITE_OK) {
    return false;
  }

  if (sqlite3_exec(db_, "CREATE TABLE list_names ("
      "id INTEGER PRIMARY KEY AUTOINCREMENT,"
      "name TEXT)",
      NULL, NULL, NULL) != SQLITE_OK) {
    return false;
  }

  sqlite3_exec(db_, "CREATE INDEX chunks_chunk_id ON chunks(chunk_id)",
               NULL, NULL, NULL);

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
  return true;
}

// The SafeBrowsing service assumes this operation is synchronous.
bool SafeBrowsingDatabaseImpl::ResetDatabase() {
  hash_cache_->clear();
  prefix_miss_cache_.clear();

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

bool SafeBrowsingDatabaseImpl::CheckCompatibleVersion() {
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

bool SafeBrowsingDatabaseImpl::ContainsUrl(
    const GURL& url,
    std::string* matching_list,
    std::vector<SBPrefix>* prefix_hits,
    std::vector<SBFullHashResult>* full_hits,
    Time last_update) {
  matching_list->clear();
  prefix_hits->clear();
  if (!init_) {
    DCHECK(false);
    return false;
  }

  if (!url.is_valid())
    return false;

  std::vector<std::string> hosts, paths;
  safe_browsing_util::GenerateHostsToCheck(url, &hosts);
  safe_browsing_util::GeneratePathsToCheck(url, &paths);
  if (hosts.size() == 0)
    return false;

  // Per the spec, if there is at least 3 components, check both the most
  // significant three components and the most significant two components.
  // If only two components, check the most significant two components.
  // If it's an IP address, use the entire IP address as the host.
  SBPrefix host_key_2, host_key_3, host_key_ip;
  if (url.HostIsIPAddress()) {
    base::SHA256HashString(url.host() + "/", &host_key_ip, sizeof(SBPrefix));
    CheckUrl(url.host(), host_key_ip, paths, matching_list, prefix_hits);
  } else {
    base::SHA256HashString(hosts[0] + "/", &host_key_2, sizeof(SBPrefix));
    if (hosts.size() > 1)
      base::SHA256HashString(hosts[1] + "/", &host_key_3, sizeof(SBPrefix));

    for (size_t i = 0; i < hosts.size(); ++i) {
      SBPrefix host_key = i == 0 ? host_key_2 : host_key_3;
      CheckUrl(hosts[i], host_key, paths, matching_list, prefix_hits);
    }
  }

  if (!matching_list->empty() || !prefix_hits->empty()) {
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
    GetCachedFullHashes(prefix_hits, full_hits, last_update);
    return true;
  }

  // Check if we're getting too many FPs in the bloom filter, in which case
  // it's time to rebuild it.
  bloom_filter_fp_count_++;
  if (!bloom_filter_building_ &&
      bloom_filter_read_count_ > kBloomFilterMinReadsToCheckFP) {
    double fp_rate = bloom_filter_fp_count_ * 100 / bloom_filter_read_count_;
    if (fp_rate > kBloomFilterMaxFPRate) {
      DeleteBloomFilter();
      MessageLoop::current()->PostTask(FROM_HERE,
          bloom_read_factory_.NewRunnableMethod(
              &SafeBrowsingDatabaseImpl::BuildBloomFilter));
    }
  }

  return false;
}

void SafeBrowsingDatabaseImpl::CheckUrl(const std::string& host,
                                        SBPrefix host_key,
                                        const std::vector<std::string>& paths,
                                        std::string* matching_list,
                                        std::vector<SBPrefix>* prefix_hits) {
  // First see if there are any entries in the db for this host.
  SBHostInfo info;
  if (!ReadInfo(host_key, &info, NULL))
    return;  // No hostkey found.  This is definitely safe.

  std::vector<SBFullHash> prefixes;
  prefixes.resize(paths.size());
  for (size_t i = 0; i < paths.size(); ++i)
    base::SHA256HashString(host + paths[i], &prefixes[i], sizeof(SBFullHash));

  std::vector<SBPrefix> hits;
  int list_id = -1;
  if (!info.Contains(prefixes, &list_id, &hits))
    return;

  if (list_id != -1) {
    *matching_list = GetListName(list_id);
  } else if (hits.empty()) {
    prefix_hits->push_back(host_key);
  } else {
    for (size_t i = 0; i < hits.size(); ++i)
      prefix_hits->push_back(hits[i]);
  }
}

bool SafeBrowsingDatabaseImpl::ReadInfo(int host_key,
                                        SBHostInfo* info,
                                        int* id) {
  STATS_COUNTER("SB.HostSelect", 1);
  SQLITE_UNIQUE_STATEMENT(statement, *statement_cache_,
      "SELECT id, entries FROM hosts WHERE host=?");
  if (!statement.is_valid()) {
    NOTREACHED();
    return false;
  }

  statement->bind_int(0, host_key);
  int result = statement->step();
  if (result == SQLITE_CORRUPT) {
    HandleCorruptDatabase();
    return false;
  }

  if (result == SQLITE_DONE)
    return false;

  if (result != SQLITE_ROW) {
    DLOG(ERROR) << "SafeBrowsingDatabaseImpl got "
        "statement->step() != SQLITE_ROW for "
        << host_key;
    return false;
  }

  if (id)
    *id = statement->column_int(0);

  return info->Initialize(statement->column_blob(1),
                          statement->column_bytes(1));
}

void SafeBrowsingDatabaseImpl::WriteInfo(int host_key,
                                         const SBHostInfo& info,
                                         int id) {
  SQLITE_UNIQUE_STATEMENT(statement1, *statement_cache_,
      "INSERT OR REPLACE INTO hosts"
      "(host, entries)"
      "VALUES (?,?)");

  SQLITE_UNIQUE_STATEMENT(statement2, *statement_cache_,
      "INSERT OR REPLACE INTO hosts"
      "(id, host, entries)"
      "VALUES (?,?,?)");

  SqliteCompiledStatement& statement = id == 0 ? statement1 : statement2;
  if (!statement.is_valid()) {
    NOTREACHED();
    return;
  }

  int start_index = 0;
  if (id != 0) {
    statement->bind_int(start_index++, id);
    STATS_COUNTER("SB.HostReplace", 1);
  } else {
    STATS_COUNTER("SB.HostInsert", 1);
  }

  statement->bind_int(start_index++, host_key);
  statement->bind_blob(start_index++, info.data(), info.size());
  int rv = statement->step();
  if (rv == SQLITE_CORRUPT) {
    HandleCorruptDatabase();
  } else {
    DCHECK(rv == SQLITE_DONE);
  }
  AddHostToBloomFilter(host_key);
}

void SafeBrowsingDatabaseImpl::DeleteInfo(int host_key) {
  STATS_COUNTER("SB.HostDelete", 1);
  SQLITE_UNIQUE_STATEMENT(statement, *statement_cache_,
      "DELETE FROM hosts WHERE host=?");
  if (!statement.is_valid()) {
    NOTREACHED();
    return;
  }

  statement->bind_int(0, host_key);
  int rv = statement->step();
  if (rv == SQLITE_CORRUPT) {
    HandleCorruptDatabase();
  } else {
    DCHECK(rv == SQLITE_DONE);
  }
}

void SafeBrowsingDatabaseImpl::StartThrottledWork() {
  if (process_factory_.empty())
    RunThrottledWork();
}

void SafeBrowsingDatabaseImpl::RunThrottledWork() {
  prefix_miss_cache_.clear();
  while (true) {
    bool done = ProcessChunks();

    if (done)
      done = ProcessAddDel();

    if (done)
      break;

    if (asynchronous_) {
      // For production code, we want to throttle by calling InvokeLater to
      // continue the work after a delay.  However for unit tests we depend on
      // updates to happen synchronously.
      MessageLoop::current()->PostDelayedTask(FROM_HERE,
          process_factory_.NewRunnableMethod(
              &SafeBrowsingDatabaseImpl::RunThrottledWork), disk_delay_);
      break;
    } else {
      PlatformThread::Sleep(kMaxThreadHoldupMs);
    }
  }
}

void SafeBrowsingDatabaseImpl::InsertChunks(const std::string& list_name,
                                            std::deque<SBChunk>* chunks) {
  // We've going to be updating the bloom filter, so delete the on-disk
  // serialization so that if the process crashes we'll generate a new one on
  // startup, instead of reading a stale filter.
  DeleteBloomFilter();

  int list_id = GetListID(list_name);
  std::deque<SBChunk>::iterator i = chunks->begin();
  for (; i != chunks->end(); ++i) {
    SBChunk& chunk = (*i);
    chunk.list_id = list_id;
    std::deque<SBChunkHost>::iterator j = chunk.hosts.begin();
    for (; j != chunk.hosts.end(); ++j) {
      j->entry->set_list_id(list_id);
      if (j->entry->IsAdd())
        j->entry->set_chunk_id(chunk.chunk_number);
    }
  }

  pending_chunks_.push(chunks);

  BeginTransaction();
  StartThrottledWork();
}

bool SafeBrowsingDatabaseImpl::ProcessChunks() {
  if (pending_chunks_.empty())
    return true;

  while (!pending_chunks_.empty()) {
    std::deque<SBChunk>* chunks = pending_chunks_.front();
    bool done = false;
    if (chunks->front().is_add) {
      done = ProcessAddChunks(chunks);
    } else {
      done = ProcessSubChunks(chunks);
    }

    if (!done)
      return false;

    delete chunks;
    pending_chunks_.pop();
    EndTransaction();
  }

  if (!bloom_filter_building_) {
    if (asynchronous_) {
      // When we're updating, there will usually be a bunch of pending_chunks_
      // to process, and we don't want to keep writing the bloom filter to disk
      // 10 or 20 times unnecessarily.  So schedule to write it in a minute, and
      // if any new updates happen in the meantime, push that forward.
      if (!bloom_write_factory_.empty())
        bloom_write_factory_.RevokeAll();

      MessageLoop::current()->PostDelayedTask(FROM_HERE,
          bloom_write_factory_.NewRunnableMethod(
              &SafeBrowsingDatabaseImpl::WriteBloomFilter),
          kBloomFilterWriteDelayMs);
    } else {
      WriteBloomFilter();
    }
  }

  if (chunk_inserted_callback_.get())
    chunk_inserted_callback_->Run();

  return true;
}

bool SafeBrowsingDatabaseImpl::ProcessAddChunks(std::deque<SBChunk>* chunks) {
  Time before = Time::Now();
  while (!chunks->empty()) {
    SBChunk& chunk = chunks->front();
    int list_id = chunk.list_id;
    int chunk_id = chunk.chunk_number;

    // The server can give us a chunk that we already have because it's part of
    // a range.  Don't add it again.
    if (!ChunkExists(list_id, ADD_CHUNK, chunk_id)) {
      while (!chunk.hosts.empty()) {
        // Read the existing record for this host, if it exists.
        SBPrefix host = chunk.hosts.front().host;
        SBEntry* entry = chunk.hosts.front().entry;

        UpdateInfo(host, entry, false);

        if (!add_chunk_modified_hosts_.empty())
          add_chunk_modified_hosts_.append(",");

        add_chunk_modified_hosts_.append(StringPrintf("%d", host));

        entry->Destroy();
        chunk.hosts.pop_front();
        if (!chunk.hosts.empty() &&
            (Time::Now() - before).InMilliseconds() > kMaxThreadHoldupMs) {
          return false;
        }
      }

      AddChunkInformation(list_id, ADD_CHUNK, chunk_id,
                          add_chunk_modified_hosts_);
      add_chunk_modified_hosts_.clear();
    } else {
      while (!chunk.hosts.empty()) {
        chunk.hosts.front().entry->Destroy();
        chunk.hosts.pop_front();
      }
    }

    chunks->pop_front();
  }

  return true;
}

bool SafeBrowsingDatabaseImpl::ProcessSubChunks(std::deque<SBChunk>* chunks) {
  Time before = Time::Now();
  while (!chunks->empty()) {
    SBChunk& chunk = chunks->front();
    int list_id = chunk.list_id;
    int chunk_id = chunk.chunk_number;

    if (!ChunkExists(list_id, SUB_CHUNK, chunk_id)) {
      while (!chunk.hosts.empty()) {
        SBPrefix host = chunk.hosts.front().host;
        SBEntry* entry = chunk.hosts.front().entry;
        UpdateInfo(host, entry, true);

        entry->Destroy();
        chunk.hosts.pop_front();
        if (!chunk.hosts.empty() &&
            (Time::Now() - before).InMilliseconds() > kMaxThreadHoldupMs) {
          return false;
        }
      }

      AddChunkInformation(list_id, SUB_CHUNK, chunk_id, "");
    } else {
      while (!chunk.hosts.empty()) {
        chunk.hosts.front().entry->Destroy();
        chunk.hosts.pop_front();
      }
    }

    chunks->pop_front();
  }

  return true;
}

void SafeBrowsingDatabaseImpl::UpdateInfo(SBPrefix host_key,
                                          SBEntry* entry,
                                          bool persist) {
  // If an existing record exists, and the new record is smaller, then reuse
  // its entry to reduce database fragmentation.
  int old_id = 0;
  SBHostInfo info;
  // If the bloom filter isn't there, then assume that the entry exists,
  // otherwise test the bloom filter.
  bool exists = !bloom_filter_.get() || bloom_filter_->Exists(host_key);
  if (exists)
    exists = ReadInfo(host_key, &info, &old_id);
  int old_size = info.size();

  if (entry->IsAdd()) {
    info.AddPrefixes(entry);
  } else {
    ClearCachedHashes(entry);
    info.RemovePrefixes(entry, persist);
  }

  if (old_size == info.size()) {
    // The entry didn't change, so no point writing it.
    return;
  }

  if (!info.size()) {
    // Just delete the existing information instead of writing an empty one.
    if (exists)
      DeleteInfo(host_key);
    return;
  }

  if (info.size() > old_size) {
    // New record is larger, so just add a new entry.
    old_id = 0;
  }

  WriteInfo(host_key, info, old_id);
}

void SafeBrowsingDatabaseImpl::DeleteChunks(
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
    // Only start a transaction for pending AddDel work if we haven't started
    // one already.
    BeginTransaction();
    StartThrottledWork();
  }

  delete chunk_deletes;
  EndTransaction();
}

void SafeBrowsingDatabaseImpl::AddDel(const std::string& list_name,
                                      int add_chunk_id) {
  STATS_COUNTER("SB.ChunkSelect", 1);
  int list_id = GetListID(list_name);
  // Find all the prefixes that came from the given add_chunk_id.
  SQLITE_UNIQUE_STATEMENT(statement, *statement_cache_,
      "SELECT hostkeys FROM chunks WHERE "
      "list_id=? AND chunk_type=? AND chunk_id=?");
  if (!statement.is_valid()) {
    NOTREACHED();
    return;
  }

  std::string hostkeys_str;
  statement->bind_int(0, list_id);
  statement->bind_int(1, ADD_CHUNK);
  statement->bind_int(2, add_chunk_id);
  int rv = statement->step();
  if (rv != SQLITE_ROW || !statement->column_string(0, &hostkeys_str)) {
    if (rv == SQLITE_CORRUPT) {
      HandleCorruptDatabase();
    } else {
      NOTREACHED();
    }

    return;
  }

  AddDelWork work;
  work.list_id = list_id;
  work.add_chunk_id = add_chunk_id;
  pending_add_del_.push(work);
  SplitString(hostkeys_str, ',',  &pending_add_del_.back().hostkeys);
}

bool SafeBrowsingDatabaseImpl::ProcessAddDel() {
  if (pending_add_del_.empty())
    return true;

  Time before = Time::Now();
  while (!pending_add_del_.empty()) {
    AddDelWork& add_del_work = pending_add_del_.front();
    ClearCachedHashesForChunk(add_del_work.list_id, add_del_work.add_chunk_id);
    std::vector<std::string>& hostkeys = add_del_work.hostkeys;
    for (size_t i = 0; i < hostkeys.size(); ++i) {
      SBPrefix host = atoi(hostkeys[i].c_str());
      // Doesn't matter if we use SUB_PREFIX or SUB_FULL_HASH since if there
      // are no prefixes it's not used.
      SBEntry* entry = SBEntry::Create(SBEntry::SUB_PREFIX, 0);
      entry->set_list_id(add_del_work.list_id);
      entry->set_chunk_id(add_del_work.add_chunk_id);
      UpdateInfo(host, entry, false);
      entry->Destroy();
      if ((Time::Now() - before).InMilliseconds() > kMaxThreadHoldupMs) {
        hostkeys.erase(hostkeys.begin(), hostkeys.begin() + i);
        return false;
      }
    }

    RemoveChunkId(add_del_work.list_id, ADD_CHUNK, add_del_work.add_chunk_id);
    pending_add_del_.pop();
  }

  EndTransaction();

  return true;
}

void SafeBrowsingDatabaseImpl::SubDel(const std::string& list_name,
                                      int sub_chunk_id) {
  RemoveChunkId(GetListID(list_name), SUB_CHUNK, sub_chunk_id);
}

void SafeBrowsingDatabaseImpl::AddChunkInformation(
    int list_id, ChunkType type, int chunk_id, const std::string& hostkeys) {
  STATS_COUNTER("SB.ChunkInsert", 1);
  SQLITE_UNIQUE_STATEMENT(statement, *statement_cache_,
      "INSERT INTO chunks (list_id, chunk_type, chunk_id, hostkeys) "
      "VALUES (?,?,?,?)");
  if (!statement.is_valid()) {
    NOTREACHED();
    return;
  }

  statement->bind_int(0, list_id);
  statement->bind_int(1, type);
  statement->bind_int(2, chunk_id);
  statement->bind_string(3, hostkeys);
  int rv = statement->step();
  if (rv == SQLITE_CORRUPT) {
    HandleCorruptDatabase();
  } else {
    DCHECK(rv == SQLITE_DONE);
  }
}

void SafeBrowsingDatabaseImpl::GetListsInfo(
    std::vector<SBListChunkRanges>* lists) {
  lists->clear();
  SQLITE_UNIQUE_STATEMENT(statement, *statement_cache_,
      "SELECT name,id FROM list_names");
  if (!statement.is_valid()) {
    NOTREACHED();
    return;
  }

  while (true) {
    int rv = statement->step();
    if (rv != SQLITE_ROW) {
      if (rv == SQLITE_CORRUPT)
        HandleCorruptDatabase();

      break;
    }
    int list_id = statement->column_int(1);
    lists->push_back(SBListChunkRanges(statement->column_string(0)));
    GetChunkIds(list_id, ADD_CHUNK, &lists->back().adds);
    GetChunkIds(list_id, SUB_CHUNK, &lists->back().subs);
  }
}

void SafeBrowsingDatabaseImpl::GetChunkIds(int list_id,
                                           ChunkType type,
                                           std::string* list) {
  list->clear();
  STATS_COUNTER("SB.ChunkSelect", 1);
  SQLITE_UNIQUE_STATEMENT(statement, *statement_cache_,
      "SELECT chunk_id FROM chunks WHERE list_id=? AND chunk_type=? "
      "ORDER BY chunk_id");
  if (!statement.is_valid()) {
    NOTREACHED();
    return;
  }

  statement->bind_int(0, list_id);
  statement->bind_int(1, type);

  std::vector<int> chunk_ids;
  while (true) {
    int rv = statement->step();
    if (rv != SQLITE_ROW) {
      if (rv == SQLITE_CORRUPT)
        HandleCorruptDatabase();

      break;
    }
    chunk_ids.push_back(statement->column_int(0));
  }

  std::vector<ChunkRange> ranges;
  ChunksToRanges(chunk_ids, &ranges);
  RangesToString(ranges, list);
}

bool SafeBrowsingDatabaseImpl::ChunkExists(int list_id,
                                           ChunkType type,
                                           int chunk_id) {
  STATS_COUNTER("SB.ChunkSelect", 1);
  SQLITE_UNIQUE_STATEMENT(statement, *statement_cache_,
      "SELECT chunk_id FROM chunks WHERE"
      " list_id=? AND chunk_type=? AND chunk_id=?");
  if (!statement.is_valid()) {
    NOTREACHED();
    return false;
  }

  statement->bind_int(0, list_id);
  statement->bind_int(1, type);
  statement->bind_int(2, chunk_id);

  int rv = statement->step();
  if (rv == SQLITE_CORRUPT)
    HandleCorruptDatabase();

  return rv == SQLITE_ROW;
}

void SafeBrowsingDatabaseImpl::RemoveChunkId(int list_id,
                                             ChunkType type,
                                             int chunk_id) {
  // Also remove the add chunk id from add_chunks
  STATS_COUNTER("SB.ChunkDelete", 1);
  SQLITE_UNIQUE_STATEMENT(statement, *statement_cache_,
      "DELETE FROM chunks WHERE list_id=? AND chunk_type=? AND chunk_id=?");
  if (!statement.is_valid()) {
    NOTREACHED();
    return;
  }

  statement->bind_int(0, list_id);
  statement->bind_int(1, type);
  statement->bind_int(2, chunk_id);
  int rv = statement->step();
  if (rv == SQLITE_CORRUPT) {
    HandleCorruptDatabase();
  } else {
    DCHECK(rv == SQLITE_DONE);
  }
}

int SafeBrowsingDatabaseImpl::AddList(const std::string& name) {
  SQLITE_UNIQUE_STATEMENT(statement, *statement_cache_,
      "INSERT INTO list_names (id, name) "
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

int SafeBrowsingDatabaseImpl::GetListID(const std::string& name) {
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

std::string SafeBrowsingDatabaseImpl::GetListName(int id) {
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

void SafeBrowsingDatabaseImpl::AddHostToBloomFilter(int host_key) {
  if (bloom_filter_building_)
    bloom_filter_temp_hostkeys_.push_back(host_key);
  // Even if we're rebuilding the bloom filter, we still need to update the
  // current one since we also use it to decide whether to do certain database
  // operations during update.
  if (bloom_filter_.get())
    bloom_filter_->Insert(host_key);
}

void SafeBrowsingDatabaseImpl::BuildBloomFilter() {
  // A bloom filter needs the size at creation, however doing a select count(*)
  // is too slow since sqlite would have to enumerate each entry to get the
  // count.  So instead we load all the hostkeys into memory, and then when
  // we've read all of them and have the total count, we can create the bloom
  // filter.
  bloom_filter_temp_hostkeys_.reserve(kBloomFilterMinSize);

  bloom_filter_building_ = true;
  bloom_filter_rebuild_time_ = Time::Now();

  BeginTransaction();

  OnReadHostKeys(0);
}

void SafeBrowsingDatabaseImpl::OnReadHostKeys(int start_id) {
  // Since reading all the keys in one go could take > 20 seconds, instead we
  // read them in small chunks.
  STATS_COUNTER("SB.HostSelectForBloomFilter", 1);
  SQLITE_UNIQUE_STATEMENT(statement, *statement_cache_,
      "SELECT host,id FROM hosts WHERE id > ? ORDER BY id");
  if (!statement.is_valid()) {
    NOTREACHED();
    return;
  }

  statement->bind_int(0, start_id);
  Time before = Time::Now();
  int count = 0;

  int next_id = start_id + 1;
  while (true) {
    int rv = statement->step();
    if (rv != SQLITE_ROW) {
      if (rv == SQLITE_CORRUPT)
        HandleCorruptDatabase();

      break;
    }

    count++;
    bloom_filter_temp_hostkeys_.push_back(statement->column_int(0));
    next_id = statement->column_int(1) + 1;
    if ((Time::Now() - before).InMilliseconds() > kMaxThreadHoldupMs) {
      if (asynchronous_) {
        break;
      } else {
        PlatformThread::Sleep(kMaxThreadHoldupMs);
      }
    }
  }

  TimeDelta chunk_time = Time::Now() - before;
  int time_ms = static_cast<int>(chunk_time.InMilliseconds());
  SB_DLOG(INFO) << "SafeBrowsingDatabaseImpl read " << count
                << " hostkeys in " << time_ms << " ms";

  if (!count || !asynchronous_) {
    OnDoneReadingHostKeys();
    return;
  }

  // To avoid hammering the disk and disrupting other parts of Chrome that use
  // the disk, we throttle the rebuilding.
  MessageLoop::current()->PostDelayedTask(FROM_HERE,
      bloom_read_factory_.NewRunnableMethod(
          &SafeBrowsingDatabaseImpl::OnReadHostKeys, next_id),
      disk_delay_);
}

void SafeBrowsingDatabaseImpl::OnDoneReadingHostKeys() {
  EndTransaction();
  Time before = Time::Now();
  int number_of_keys = std::max(kBloomFilterMinSize,
      static_cast<int>(bloom_filter_temp_hostkeys_.size()));
  int filter_size = number_of_keys * kBloomFilterSizeRatio;
  BloomFilter* filter = new BloomFilter(filter_size);
  for (size_t i = 0; i < bloom_filter_temp_hostkeys_.size(); ++i)
    filter->Insert(bloom_filter_temp_hostkeys_[i]);

  bloom_filter_ = filter;

  TimeDelta bloom_gen = Time::Now() - before;
  TimeDelta delta = Time::Now() - bloom_filter_rebuild_time_;
  SB_DLOG(INFO) << "SafeBrowsingDatabaseImpl built bloom filter in " <<
      delta.InMilliseconds() << " ms total (" << bloom_gen.InMilliseconds()
      << " ms to generate bloom filter).  hostkey count: " <<
      bloom_filter_temp_hostkeys_.size();

  WriteBloomFilter();
  bloom_filter_building_ = false;
  bloom_filter_temp_hostkeys_.clear();
  bloom_filter_read_count_ = 0;
  bloom_filter_fp_count_ = 0;
}

void SafeBrowsingDatabaseImpl::BeginTransaction() {
  transaction_count_++;
  if (transaction_.get() == NULL) {
    transaction_.reset(new SQLTransaction(db_));
    if (transaction_->Begin() != SQLITE_OK) {
      DCHECK(false) << "Safe browsing database couldn't start transaction";
      transaction_.reset();
    }
  }
}

void SafeBrowsingDatabaseImpl::EndTransaction() {
  if (--transaction_count_ == 0) {
    if (transaction_.get() != NULL) {
      STATS_COUNTER("SB.TransactionCommit", 1);
      transaction_->Commit();
      transaction_.reset();
    }
  }
}

void SafeBrowsingDatabaseImpl::GetCachedFullHashes(
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
        hash_cache_->erase(hit);
    }
  }
}

void SafeBrowsingDatabaseImpl::CacheHashResults(
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
    HashList& entries = (*hash_cache_)[prefix];
    HashCacheEntry entry;
    entry.received = now;
    entry.list_id = GetListID(it->list_name);
    entry.add_chunk_id = it->add_chunk_id;
    memcpy(&entry.full_hash, &it->hash.full_hash, sizeof(SBFullHash));
    entries.push_back(entry);
  }
}

void SafeBrowsingDatabaseImpl::ClearCachedHashes(const SBEntry* entry) {
  for (int i = 0; i < entry->prefix_count(); ++i) {
    SBPrefix prefix;
    if (entry->type() == SBEntry::SUB_FULL_HASH)
      memcpy(&prefix, &entry->FullHashAt(i), sizeof(SBPrefix));
    else
      prefix = entry->PrefixAt(i);

    HashCache::iterator it = hash_cache_->find(prefix);
    if (it != hash_cache_->end())
      hash_cache_->erase(it);
  }
}

// This clearing algorithm is a little inefficient, but we don't expect there to
// be too many entries for this to matter. Also, this runs as a background task
// during an update, so no user action is blocking on it.
void SafeBrowsingDatabaseImpl::ClearCachedHashesForChunk(int list_id,
                                                         int add_chunk_id) {
  HashCache::iterator it = hash_cache_->begin();
  while (it != hash_cache_->end()) {
    HashList& entries = it->second;
    HashList::iterator eit = entries.begin();
    while (eit != entries.end()) {
      if (eit->list_id == list_id && eit->add_chunk_id == add_chunk_id)
        eit = entries.erase(eit);
      else
        ++eit;
    }
    if (entries.empty())
      hash_cache_->erase(it++);
    else
      ++it;
  }
}

void SafeBrowsingDatabaseImpl::HandleCorruptDatabase() {
  MessageLoop::current()->PostTask(FROM_HERE,
      reset_factory_.NewRunnableMethod(
          &SafeBrowsingDatabaseImpl::OnHandleCorruptDatabase));
}

void SafeBrowsingDatabaseImpl::OnHandleCorruptDatabase() {
  ResetDatabase();
  DCHECK(false) << "SafeBrowsing database was corrupt and reset";
}

void SafeBrowsingDatabaseImpl::HandleResume() {
  disk_delay_ = kOnResumeHoldupMs;
  MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      resume_factory_.NewRunnableMethod(
          &SafeBrowsingDatabaseImpl::OnResumeDone),
      kOnResumeHoldupMs);
}

void SafeBrowsingDatabaseImpl::OnResumeDone() {
  disk_delay_ = kMaxThreadHoldupMs;
}

void SafeBrowsingDatabaseImpl::SetSynchronous() {
  asynchronous_ = false;
}
