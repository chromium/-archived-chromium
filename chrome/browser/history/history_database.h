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

#ifndef CHROME_BROWSER_HISTORY_HISTORY_DATABASE_H__
#define CHROME_BROWSER_HISTORY_HISTORY_DATABASE_H__

#include "chrome/browser/history/download_database.h"
#include "chrome/browser/history/history.h"
#include "chrome/browser/history/history_types.h"
#include "chrome/browser/history/starred_url_database.h"
#include "chrome/browser/history/url_database.h"
#include "chrome/browser/history/visit_database.h"
#include "chrome/browser/history/visitsegment_database.h"
#include "chrome/browser/meta_table_helper.h"
#include "chrome/common/sqlite_compiled_statement.h"
#include "chrome/common/sqlite_utils.h"

struct sqlite3;

namespace history {

// Forward declaration for the temporary migration code in Init().
class TextDatabaseManager;

// Encapsulates the SQL connection for the history database. This class holds
// the database connection and has methods the history system (including full
// text search) uses for writing and retrieving information.
//
// We try to keep most logic out of the history database; this should be seen
// as the storage interface. Logic for manipulating this storage layer should
// be in HistoryBackend.cc.
class HistoryDatabase : public DownloadDatabase,
                        public StarredURLDatabase,
                        public VisitDatabase,
                        public VisitSegmentDatabase {
 public:
  // A simple class for scoping a history database transaction. This does not
  // support rollback since the history database doesn't, either.
  class TransactionScoper {
   public:
    TransactionScoper(HistoryDatabase* db) : db_(db) {
      db_->BeginTransaction();
    }
    ~TransactionScoper() {
      db_->CommitTransaction();
    }
   private:
    HistoryDatabase* db_;
  };

  // Must call Init() to complete construction. Although it can be created on
  // any thread, it must be destructed on the history thread for proper
  // database cleanup.
  HistoryDatabase();

  virtual ~HistoryDatabase();

  // Must call this function to complete initialization. Will return true on
  // success. On false, no other function should be called. You may want to call
  // BeginExclusiveMode after this when you are ready.
  InitStatus Init(const std::wstring& history_name);

  // Call to set the mode on the database to exclusive. The default locking mode
  // is "normal" but we want to run in exclusive mode for slightly better
  // performance since we know nobody else is using the database. This is
  // separate from Init() since the in-memory database attaches to slurp the
  // data out, and this can't happen in exclusive mode.
  void BeginExclusiveMode();

  // Returns the current version that we will generate history databases with.
  static int GetCurrentVersion();

  // Transactions on the history database. Use the Transaction object above
  // for most work instead of these directly. We support nested transactions
  // and only commit when the outermost transaction is committed. This means
  // that it is impossible to rollback a specific transaction. We could roll
  // back the outermost transaction if any inner one is rolled back, but it
  // turns out we don't really need this type of integrity for the history
  // database, so we just don't support it.
  void BeginTransaction();
  void CommitTransaction();
  int transaction_nesting() const {  // for debugging and assertion purposes
    return transaction_nesting_;
  }

  // Drops all tables except the URL, starred and download tables, and recreates
  // them from scratch. This is done to rapidly clean up stuff when deleting all
  // history. It is faster and less likely to have problems that deleting all
  // rows in the tables.
  //
  // We don't delete the downloads table, since there may be in progress
  // downloads. We handle the download history clean up separately in:
  // DownloadManager::RemoveDownloadsFromHistoryBetween.
  //
  // Returns true on success. On failure, the caller should assume that the
  // database is invalid. There could have been an error recreating a table.
  // This should be treated the same as an init failure, and the database
  // should not be used any more.
  //
  // This will also recreate the supplimentary URL indices, since these
  // indices won't be created automatically when using the temporary URL
  // talbe (what the caller does right before calling this).
  bool RecreateAllButStarAndURLTables();

  // Vacuums the database. This will cause sqlite to defragment and collect
  // unused space in the file. It can be VERY SLOW.
  void Vacuum();

  // Visit table functions ----------------------------------------------------

  // Update the segment id of a visit. Return true on success.
  bool SetSegmentID(VisitID visit_id, SegmentID segment_id);

  // Query the segment ID for the provided visit. Return 0 on failure or if the
  // visit id wasn't found.
  SegmentID GetSegmentID(VisitID visit_id);

 private:
  // Implemented for URLDatabase.
  virtual sqlite3* GetDB();
  virtual SqliteStatementCache& GetStatementCache();

  // Primes the sqlite cache on startup by filling it with the file in sequence
  // until there is no more data or the cache is full. Since this is one
  // contiguous read operation, it is much faster than letting the pages come
  // in on-demand (which causes lots of seeks).
  void PrimeCache();

  // Sets the fields of the supplied entry from the starred select statement.
  void FillInStarredEntry(SQLStatement* s, StarredEntry* entry);

  // Migration -----------------------------------------------------------------

  // Makes sure the version is up-to-date, updating if necessary. If the
  // database is too old to migrate, the user will be notified. In this case, or
  // for other errors, false will be returned. True means it is up-to-date and
  // ready for use.
  //
  // This assumes it is called from the init function inside a transaction. It
  // may commit the transaction and start a new one if migration requires it.
  InitStatus EnsureCurrentVersion();

  // ---------------------------------------------------------------------------

  // How many nested transactions are pending? When this gets to 0, we commit.
  int transaction_nesting_;

  // The database. The closer automatically closes the deletes the db and the
  // statement cache. These must be done in a specific order, so we don't want
  // to rely on C++'s implicit destructors for the individual objects.
  //
  // The close scoper will free the database and delete the statement cache in
  // the correct order automatically when we are destroyed.
  DBCloseScoper db_closer_;
  sqlite3* db_;
  SqliteStatementCache* statement_cache_;

  MetaTableHelper meta_table_;

  DISALLOW_EVIL_CONSTRUCTORS(HistoryDatabase);
};

}  // history

#endif  // CHROME_BROWSER_HISTORY_HISTORY_DATABASE_H__
