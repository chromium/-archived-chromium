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

#ifndef CHROME_BROWSER_SAFE_BROWSING_SAFE_BROWSING_DATABASE_H__
#define CHROME_BROWSER_SAFE_BROWSING_SAFE_BROWSING_DATABASE_H__

#include <hash_map>
#include <list>
#include <queue>
#include <vector>

#include "base/scoped_ptr.h"
#include "base/task.h"
#include "base/time.h"
#include "chrome/browser/safe_browsing/safe_browsing_util.h"
#include "chrome/common/sqlite_compiled_statement.h"
#include "chrome/common/sqlite_utils.h"
#include "googleurl/src/gurl.h"

class BloomFilter;

// Encapsulates the database that stores information about phishing and malware
// sites.  There is one on-disk database for all profiles, as it doesn't
// contain user-specific data.  This object is not thread-safe, i.e. all its
// methods should be used on the same thread that it was created on, with the
// exception of NeedToCheckUrl.
class SafeBrowsingDatabase {
 public:
  SafeBrowsingDatabase();
  ~SafeBrowsingDatabase();

  // Initializes the database with the given filename.  The callback is
  // executed after finishing a chunk.
  bool Init(const std::wstring& filename,
            Callback0::Type* chunk_inserted_callback);

  // Deletes the current database and creates a new one.
  bool ResetDatabase();

  // This function can be called on any thread to check if the given url may be
  // in the database.  If this function returns false, it is definitely not in
  // the database and ContainsUrl doesn't need to be called.  If it returns
  // true, then the url might be in the database and ContainsUrl needs to be
  // called.  This function can only be called after Init succeeded.
  bool NeedToCheckUrl(const GURL& url);

  // Returns false if the given url is not in the database.  If it returns
  // true, then either "list" is the name of the matching list, or prefix_hits
  // contains the matching hash prefixes.
  bool ContainsUrl(const GURL& url,
                   std::string* matching_list,
                   std::vector<SBPrefix>* prefix_hits,
                   std::vector<SBFullHashResult>* full_hits,
                   Time last_update);

  // Processes add/sub commands.  Database will free the chunks when it's done.
  void InsertChunks(const std::string& list_name, std::deque<SBChunk>* chunks);

  // Processs adddel/subdel commands.  Database will free chunk_deletes when
  // it's done.
  void DeleteChunks(std::vector<SBChunkDelete>* chunk_deletes);

  // Returns the lists and their add/sub chunks.
  void GetListsInfo(std::vector<SBListChunkRanges>* lists);

  // Call this to make all database operations synchronous.  While useful for
  // testing, this should never be called in chrome.exe because it can lead
  // to blocking user requests.
  void set_synchronous() { asynchronous_ = false; }

  // Store the results of a GetHash response.
  void CacheHashResults(const std::vector<SBFullHashResult>& full_hits);

  // Called when the user's machine has resumed from a lower power state.
  void HandleResume();

 private:
  friend class SafeBrowsing_HashCaching_Test;

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

  static std::wstring BloomFilterFilename(const std::wstring& db_filename);

  // Load the bloom filter off disk.  Generates one if it can't find it.
  void LoadBloomFilter();

  // Deletes the on-disk bloom filter, i.e. because it's stale.
  void DeleteBloomFilter();

  // Writes the current bloom filter to disk.
  void WriteBloomFilter();

  // Adds the host to the bloom filter.
  void AddHostToBloomFilter(int host_key);

  // Generate a bloom filter.
  void BuildBloomFilter();

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
                           Time last_update);

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

  std::wstring filename_;

  // Used by the bloom filter.
  std::wstring bloom_filter_filename_;
  scoped_ptr<BloomFilter> bloom_filter_;
  int bloom_filter_read_count_;
  int bloom_filter_fp_count_;

  // These are temp variables used when rebuilding the bloom filter.
  bool bloom_filter_building_;
  std::vector<int> bloom_filter_temp_hostkeys_;
  int bloom_filter_last_hostkey_;
  Time bloom_filter_rebuild_time_;

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

  // Controls whether database writes are done synchronously in one go or
  // asynchronously in small chunks.
  bool asynchronous_;

  // Called after an add/sub chunk is processed.
  Callback0::Type* chunk_inserted_callback_;

  // Used to schedule small bits of work when writing to the database.
  ScopedRunnableMethodFactory<SafeBrowsingDatabase> process_factory_;

  // Used to schedule reading the database to rebuild the bloom filter.
  ScopedRunnableMethodFactory<SafeBrowsingDatabase> bloom_read_factory_;

  // Used to schedule writing the bloom filter after an update.
  ScopedRunnableMethodFactory<SafeBrowsingDatabase> bloom_write_factory_;

  // Used to schedule resetting the database because of corruption.
  ScopedRunnableMethodFactory<SafeBrowsingDatabase> reset_factory_;

  // Used to schedule resuming from a lower power state.
  ScopedRunnableMethodFactory<SafeBrowsingDatabase> resume_factory_;

  // Used for caching GetHash results.
  typedef struct HashCacheEntry {
    SBFullHash full_hash;
    int list_id;
    int add_chunk_id;
    Time received;
  } HashCacheEntry;

  typedef std::list<HashCacheEntry> HashList;
  typedef stdext::hash_map<SBPrefix, HashList> HashCache;
  HashCache hash_cache_;

  // The amount of time, in milliseconds, to wait before the next disk write.
  int disk_delay_;

  DISALLOW_EVIL_CONSTRUCTORS(SafeBrowsingDatabase);
};

#endif  // CHROME_BROWSER_SAFE_BROWSING_SAFE_BROWSING_DATABASE_H__