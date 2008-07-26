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

#include "base/string_util.h"
#include "chrome/browser/history/archived_database.h"

namespace history {

namespace {

static const int kDatabaseVersion = 1;

}  // namespace

ArchivedDatabase::ArchivedDatabase()
    : db_(NULL),
      transaction_nesting_(0) {
}

ArchivedDatabase::~ArchivedDatabase() {
}

bool ArchivedDatabase::Init(const std::wstring& file_name) {
  // Open the history database, using the narrow version of open indicates to
  // sqlite that we want the database to be in UTF-8 if it doesn't already
  // exist.
  DCHECK(!db_) << "Already initialized!";
  if (sqlite3_open(WideToUTF8(file_name).c_str(), &db_) != SQLITE_OK)
    return false;
  statement_cache_ = new SqliteStatementCache(db_);
  DBCloseScoper scoper(&db_, &statement_cache_);

  // Set the database page size to something a little larger to give us
  // better performance (we're typically seek rather than bandwidth limited).
  // This only has an effect before any tables have been created, otherwise
  // this is a NOP. Must be a power of 2 and a max of 8192.
  sqlite3_exec(db_, "PRAGMA page_size=4096", NULL, NULL, NULL);

  // Don't use very much memory caching this database. We seldom use it for
  // anything important.
  sqlite3_exec(db_, "PRAGMA cache_size=64", NULL, NULL, NULL);

  // Run the database in exclusive mode. Nobody else should be accessing the
  // database while we're running, and this will give somewhat improved perf.
  sqlite3_exec(db_, "PRAGMA locking_mode=EXCLUSIVE", NULL, NULL, NULL);

  BeginTransaction();

  // Version check.
  if (!meta_table_.Init(std::string(), kDatabaseVersion, db_))
    return false;
  if (meta_table_.GetCompatibleVersionNumber() > kDatabaseVersion) {
    // We ignore this error and just run without the database. Normally, if
    // the user is running two versions, the main history DB will give a
    // warning about a version from the future.
    LOG(WARNING) << "Archived database is a future version.";
    return false;
  }

  // Create the tables.
  if (!CreateURLTable(false) || !InitVisitTable() ||
      !InitKeywordSearchTermsTable())
    return false;
  CreateMainURLIndex();

  // Succeeded: keep the DB open by detaching the auto-closer.
  scoper.Detach();
  db_closer_.Attach(&db_, &statement_cache_);
  CommitTransaction();
  return true;
}

void ArchivedDatabase::BeginTransaction() {
  DCHECK(db_);
  if (transaction_nesting_ == 0) {
    int rv = sqlite3_exec(db_, "BEGIN TRANSACTION", NULL, NULL, NULL);
    DCHECK(rv == SQLITE_OK) << "Failed to begin transaction";
  }
  transaction_nesting_++;
}

void ArchivedDatabase::CommitTransaction() {
  DCHECK(db_);
  DCHECK(transaction_nesting_ > 0) << "Committing too many transactions";
  transaction_nesting_--;
  if (transaction_nesting_ == 0) {
    int rv = sqlite3_exec(db_, "COMMIT", NULL, NULL, NULL);
    DCHECK(rv == SQLITE_OK) << "Failed to commit transaction";
  }
}

sqlite3* ArchivedDatabase::GetDB() {
  return db_;
}

SqliteStatementCache& ArchivedDatabase::GetStatementCache() {
  return *statement_cache_;
}

}  // namespace history
