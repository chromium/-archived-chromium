// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_SAFE_BROWSING_DATABASE_IMPL_H_
#define CHROME_BROWSER_SAFE_BROWSING_SAFE_BROWSING_DATABASE_IMPL_H_

#include <deque>
#include <list>
#include <queue>
#include <set>
#include <string>
#include <vector>

#include "base/hash_tables.h"
#include "base/scoped_ptr.h"
#include "base/task.h"
#include "base/time.h"
#include "chrome/browser/safe_browsing/safe_browsing_database.h"
#include "chrome/browser/safe_browsing/safe_browsing_util.h"
#include "chrome/common/sqlite_compiled_statement.h"
#include "chrome/common/sqlite_utils.h"

// The reference implementation database using SQLite.
class SafeBrowsingDatabaseImpl : public SafeBrowsingDatabase {
 public:
  SafeBrowsingDatabaseImpl();
  virtual ~SafeBrowsingDatabaseImpl();

  // SafeBrowsingDatabase interface:

  // Initializes the database with the given filename.  The callback is
  // executed after finishing a chunk.
  virtual bool Init(const FilePath& filename,
                    Callback0::Type* chunk_inserted_callback);

  // Deletes the current database and creates a new one.
  virtual bool ResetDatabase();

  // Returns false if the given url is not in the database.  If it returns
  // true, then either "list" is the name of the matching list, or prefix_hits
  // contains the matching hash prefixes.
  virtual bool ContainsUrl(const GURL& url,
                           std::string* matching_list,
                           std::vector<SBPrefix>* prefix_hits,
                           std::vector<SBFullHashResult>* full_hits,
                           base::Time last_update);

  // Processes add/sub commands.  Database will free the chunks when it's done.
  virtual void InsertChunks(const std::string& list_name,
                            std::deque<SBChunk>* chunks);

  // Processs adddel/subdel commands.  Database will free chunk_deletes when
  // it's done.
  virtual void DeleteChunks(std::vector<SBChunkDelete>* chunk_deletes);

  // Returns the lists and their add/sub chunks.
  virtual void GetListsInfo(std::vector<SBListChunkRanges>* lists);

  virtual void SetSynchronous();

  // Store the results of a GetHash response. In the case of empty results, we
  // cache the prefixes until the next update so that we don't have to issue
  // further GetHash requests we know will be empty.
  virtual void CacheHashResults(const std::vector<SBPrefix>& prefixes,
                        const std::vector<SBFullHashResult>& full_hits);

  // Called when the user's machine has resumed from a lower power state.
  virtual void HandleResume();

 private:
  // Opens the database.
  bool Open();

  // Closes the database.
  bool Close();

  // Creates the SQL tables.
  bool CreateTables();

  // Checks the database version and if it's incompatible with the current one,
  // resets the database.
  bool CheckCompatibleVersion();

  // Updates, or adds if new, a hostkey's record with the given add/sub entry.
  // If this is a sub, removes the given prefixes, or all if prefixes is empty,
  // from host_key's record.  If persist is true, then if the add_chunk_id isn't
  // found the entry will store this sub information for future reference.
  // Otherwise the entry will not be modified if there are no matches.
  void UpdateInfo(SBPrefix host, SBEntry* entry, bool persist);

  // Returns true if any of the given prefixes exist for the given host.
  // Also returns the matching list or any prefix matches.
  void CheckUrl(const std::string& host,
                SBPrefix host_key,
                const std::vector<std::string>& paths,
                std::string* matching_list,
                std::vector<SBPrefix>* prefix_hits);

  enum ChunkType {
    ADD_CHUNK = 0,
    SUB_CHUNK = 1,
  };

  // Adds information about the given chunk to the chunks table.
  void AddChunkInformation(int list_id,
                           ChunkType type,
                           int chunk_id,
                           const std::string& hostkeys);  // only used for add

  // Return a comma separated list of chunk ids that are in the database for
  // the given list and chunk type.
  void GetChunkIds(int list_id, ChunkType type, std::string* list);

  // Checks if a chunk is in the database.
  bool ChunkExists(int list_id, ChunkType type, int chunk_id);

  // Removes the given id from our list of chunk ids.
  void RemoveChunkId(int list_id, ChunkType type, int chunk_id);

  // Reads the host's information from the database.  Returns true if it was
  // found, or false otherwise.
  bool ReadInfo(int host_key, SBHostInfo* info, int* id);

  // Writes the host's information to the database, overwriting any existing
  // information for that host_key if it existed.
  void WriteInfo(int host_key, const SBHostInfo& info, int id);

  // Deletes existing information for the given hostkey.
  void DeleteInfo(int host_key);

  // Adds the given list to the database.  Returns its row id.
  int AddList(const std::string& name);

  // Given a list name, returns its internal id.  If we haven't seen it before,
  // an id is created and stored in the database.  On error, returns 0.
  int GetListID(const std::string& name);

  // Given a list id, returns its name.
  std::string GetListName(int id);

  // Adds the host to the bloom filter.
  void AddHostToBloomFilter(int host_key);

  // Generate a bloom filter.
  virtual void BuildBloomFilter();

  virtual void IncrementBloomFilterReadCount() { ++bloom_filter_read_count_; }

  // Used when generating the bloom filter.  Reads a small number of hostkeys
  // starting at the given row id.
  void OnReadHostKeys(int start_id);

  // Called when we finished reading all the hostkeys from the database during
  // bloom filter generation.
  void OnDoneReadingHostKeys();

  void StartThrottledWork();
  void RunThrottledWork();

  // Used when processing an add-del, add chunk and sub chunk commands in small
  // batches so that the db thread is never blocked.  They return true if
  // complete, or false if there's still more work to do.
  bool ProcessChunks();
  bool ProcessAddDel();

  bool ProcessAddChunks(std::deque<SBChunk>* chunks);
  bool ProcessSubChunks(std::deque<SBChunk>* chunks);

  void BeginTransaction();
  void EndTransaction();

  // Processes an add-del command, which deletes all the prefixes that came
  // from that add chunk id.
  void AddDel(const std::string& list_name, int add_chunk_id);

  // Processes a sub-del command, which just removes the sub chunk id from
  // our list.
  void SubDel(const std::string& list_name, int sub_chunk_id);

  // Looks up any cached full hashes we may have.
  void GetCachedFullHashes(const std::vector<SBPrefix>* prefix_hits,
                           std::vector<SBFullHashResult>* full_hits,
                           base::Time last_update);

  // Remove cached entries that have prefixes contained in the entry.
  void ClearCachedHashes(const SBEntry* entry);

  // Remove all GetHash entries that match the list and chunk id from an AddDel.
  void ClearCachedHashesForChunk(int list_id, int add_chunk_id);

  void HandleCorruptDatabase();
  void OnHandleCorruptDatabase();

  // Runs a small amount of time after the machine has resumed operation from
  // a low power state.
  void OnResumeDone();

  // The database connection.
  sqlite3* db_;

  // Cache of compiled statements for our database.
  scoped_ptr<SqliteStatementCache> statement_cache_;

  int transaction_count_;
  scoped_ptr<SQLTransaction> transaction_;

  // True iff the database has been opened successfully.
  bool init_;

  // Controls whether database writes are done synchronously in one go or
  // asynchronously in small chunks.
  bool asynchronous_;

  // False positive hit rate tracking.
  int bloom_filter_fp_count_;
  int bloom_filter_read_count_;

  // These are temp variables used when rebuilding the bloom filter.
  bool bloom_filter_building_;
  std::vector<int> bloom_filter_temp_hostkeys_;
  base::Time bloom_filter_rebuild_time_;

  // Used to store throttled work for commands that write to the database.
  std::queue<std::deque<SBChunk>*> pending_chunks_;

  // Used during processing of an add chunk.
  std::string add_chunk_modified_hosts_;

  struct AddDelWork {
    int list_id;
    int add_chunk_id;
    std::vector<std::string> hostkeys;
  };

  std::queue<AddDelWork> pending_add_del_;

  // Called after an add/sub chunk is processed.
  scoped_ptr<Callback0::Type> chunk_inserted_callback_;

  // Used to schedule small bits of work when writing to the database.
  ScopedRunnableMethodFactory<SafeBrowsingDatabaseImpl> process_factory_;

  // Used to schedule reading the database to rebuild the bloom filter.
  ScopedRunnableMethodFactory<SafeBrowsingDatabaseImpl> bloom_read_factory_;

  // Used to schedule writing the bloom filter after an update.
  ScopedRunnableMethodFactory<SafeBrowsingDatabaseImpl> bloom_write_factory_;

  // Used to schedule resetting the database because of corruption.
  ScopedRunnableMethodFactory<SafeBrowsingDatabaseImpl> reset_factory_;

  // Used to schedule resuming from a lower power state.
  ScopedRunnableMethodFactory<SafeBrowsingDatabaseImpl> resume_factory_;

  // The amount of time, in milliseconds, to wait before the next disk write.
  int disk_delay_;

  DISALLOW_COPY_AND_ASSIGN(SafeBrowsingDatabaseImpl);
};

#endif  // CHROME_BROWSER_SAFE_BROWSING_SAFE_BROWSING_DATABASE_IMPL_H_
