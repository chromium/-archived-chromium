// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/string_util.h"
#include "chrome/browser/history/archived_database.h"
#include "chrome/common/sqlite_utils.h"

namespace history {

namespace {

static const int kCurrentVersionNumber = 2;
static const int kCompatibleVersionNumber = 2;

}  // namespace

ArchivedDatabase::ArchivedDatabase()
    : db_(NULL),
      transaction_nesting_(0) {
}

ArchivedDatabase::~ArchivedDatabase() {
}

bool ArchivedDatabase::Init(const FilePath& file_name) {
  // OpenSqliteDb uses the narrow version of open, indicating to sqlite that we
  // want the database to be in UTF-8 if it doesn't already exist.
  DCHECK(!db_) << "Already initialized!";
  if (OpenSqliteDb(file_name, &db_) != SQLITE_OK)
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
  if (!meta_table_.Init(std::string(), kCurrentVersionNumber,
                        kCompatibleVersionNumber, db_))
    return false;

  // Create the tables.
  if (!CreateURLTable(false) || !InitVisitTable() ||
      !InitKeywordSearchTermsTable())
    return false;
  CreateMainURLIndex();

  if (EnsureCurrentVersion() != INIT_OK)
    return false;

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

// Migration -------------------------------------------------------------------

InitStatus ArchivedDatabase::EnsureCurrentVersion() {
  // We can't read databases newer than we were designed for.
  if (meta_table_.GetCompatibleVersionNumber() > kCurrentVersionNumber) {
    LOG(WARNING) << "Archived database is too new.";
    return INIT_TOO_NEW;
  }

  // NOTICE: If you are changing structures for things shared with the archived
  // history file like URLs, visits, or downloads, that will need migration as
  // well. Instead of putting such migration code in this class, it should be
  // in the corresponding file (url_database.cc, etc.) and called from here and
  // from the archived_database.cc.

  int cur_version = meta_table_.GetVersionNumber();
  if (cur_version == 1) {
    if (!DropStarredIDFromURLs()) {
      LOG(WARNING) << "Unable to update archived database to version 2.";
      return INIT_FAILURE;
    }
    ++cur_version;
    meta_table_.SetVersionNumber(cur_version);
    meta_table_.SetCompatibleVersionNumber(
        std::min(cur_version, kCompatibleVersionNumber));
  }

  // Put future migration cases here.

  // When the version is too old, we just try to continue anyway, there should
  // not be a released product that makes a database too old for us to handle.
  LOG_IF(WARNING, cur_version < kCurrentVersionNumber) <<
      "Archived database version " << cur_version << " is too old to handle.";

  return INIT_OK;
}
}  // namespace history
