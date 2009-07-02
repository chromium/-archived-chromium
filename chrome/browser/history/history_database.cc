// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/history/history_database.h"

#include <algorithm>
#include <set>

#include "base/string_util.h"
#include "chrome/common/sqlite_utils.h"

namespace history {

namespace {

// Current version number.
static const int kCurrentVersionNumber = 16;
static const int kCompatibleVersionNumber = 16;

}  // namespace

HistoryDatabase::HistoryDatabase()
    : transaction_nesting_(0),
      db_(NULL) {
}

HistoryDatabase::~HistoryDatabase() {
}

InitStatus HistoryDatabase::Init(const FilePath& history_name,
                                 const FilePath& bookmarks_path) {
  // OpenSqliteDb uses the narrow version of open, indicating to sqlite that we
  // want the database to be in UTF-8 if it doesn't already exist.
  DCHECK(!db_) << "Already initialized!";
  if (OpenSqliteDb(history_name, &db_) != SQLITE_OK)
    return INIT_FAILURE;
  statement_cache_ = new SqliteStatementCache;
  DBCloseScoper scoper(&db_, &statement_cache_);

  // Set the database page size to something a little larger to give us
  // better performance (we're typically seek rather than bandwidth limited).
  // This only has an effect before any tables have been created, otherwise
  // this is a NOP. Must be a power of 2 and a max of 8192.
  sqlite3_exec(db_, "PRAGMA page_size=4096", NULL, NULL, NULL);

  // Increase the cache size. The page size, plus a little extra, times this
  // value, tells us how much memory the cache will use maximum.
  // 6000 * 4MB = 24MB
  // TODO(brettw) scale this value to the amount of available memory.
  sqlite3_exec(db_, "PRAGMA cache_size=6000", NULL, NULL, NULL);

  // Wrap the rest of init in a tranaction. This will prevent the database from
  // getting corrupted if we crash in the middle of initialization or migration.
  TransactionScoper transaction(this);

  // Make sure the statement cache is properly initialized.
  statement_cache_->set_db(db_);

  // Prime the cache.
  MetaTableHelper::PrimeCache(std::string(), db_);

  // Create the tables and indices.
  // NOTE: If you add something here, also add it to
  //       RecreateAllButStarAndURLTables.
  if (!meta_table_.Init(std::string(), kCurrentVersionNumber,
                        kCompatibleVersionNumber, db_))
    return INIT_FAILURE;
  if (!CreateURLTable(false) || !InitVisitTable() ||
      !InitKeywordSearchTermsTable() || !InitDownloadTable() ||
      !InitSegmentTables())
    return INIT_FAILURE;
  CreateMainURLIndex();
  CreateSupplimentaryURLIndices();

  // Version check.
  InitStatus version_status = EnsureCurrentVersion(bookmarks_path);
  if (version_status != INIT_OK)
    return version_status;

  // Succeeded: keep the DB open by detaching the auto-closer.
  scoper.Detach();
  db_closer_.Attach(&db_, &statement_cache_);
  return INIT_OK;
}

void HistoryDatabase::BeginExclusiveMode() {
  sqlite3_exec(db_, "PRAGMA locking_mode=EXCLUSIVE", NULL, NULL, NULL);
}

// static
int HistoryDatabase::GetCurrentVersion() {
  return kCurrentVersionNumber;
}

void HistoryDatabase::BeginTransaction() {
  DCHECK(db_);
  if (transaction_nesting_ == 0) {
    int rv = sqlite3_exec(db_, "BEGIN TRANSACTION", NULL, NULL, NULL);
    DCHECK(rv == SQLITE_OK) << "Failed to begin transaction";
  }
  transaction_nesting_++;
}

void HistoryDatabase::CommitTransaction() {
  DCHECK(db_);
  DCHECK(transaction_nesting_ > 0) << "Committing too many transactions";
  transaction_nesting_--;
  if (transaction_nesting_ == 0) {
    int rv = sqlite3_exec(db_, "COMMIT", NULL, NULL, NULL);
    DCHECK(rv == SQLITE_OK) << "Failed to commit transaction";
  }
}

bool HistoryDatabase::RecreateAllTablesButURL() {
  if (!DropVisitTable())
    return false;
  if (!InitVisitTable())
    return false;

  if (!DropKeywordSearchTermsTable())
    return false;
  if (!InitKeywordSearchTermsTable())
    return false;

  if (!DropSegmentTables())
    return false;
  if (!InitSegmentTables())
    return false;

  // We also add the supplementary URL indices at this point. This index is
  // over parts of the URL table that weren't automatically created when the
  // temporary URL table was
  CreateSupplimentaryURLIndices();
  return true;
}

void HistoryDatabase::Vacuum() {
  DCHECK(transaction_nesting_ == 0) <<
      "Can not have a transaction when vacuuming.";
  sqlite3_exec(db_, "VACUUM", NULL, NULL, NULL);
}

bool HistoryDatabase::SetSegmentID(VisitID visit_id, SegmentID segment_id) {
  SQLStatement s;
  if (s.prepare(db_, "UPDATE visits SET segment_id = ? WHERE id = ?") !=
      SQLITE_OK) {
    NOTREACHED();
    return false;
  }
  s.bind_int64(0, segment_id);
  s.bind_int64(1, visit_id);
  return s.step() == SQLITE_DONE;
}

SegmentID HistoryDatabase::GetSegmentID(VisitID visit_id) {
  SQLStatement s;
  if (s.prepare(db_, "SELECT segment_id FROM visits WHERE id = ?")
      != SQLITE_OK) {
    NOTREACHED();
    return 0;
  }

  s.bind_int64(0, visit_id);
  if (s.step() == SQLITE_ROW) {
    if (s.column_type(0) == SQLITE_NULL)
      return 0;
    else
      return s.column_int64(0);
  }
  return 0;
}

sqlite3* HistoryDatabase::GetDB() {
  return db_;
}

SqliteStatementCache& HistoryDatabase::GetStatementCache() {
  return *statement_cache_;
}

// Migration -------------------------------------------------------------------

InitStatus HistoryDatabase::EnsureCurrentVersion(
    const FilePath& tmp_bookmarks_path) {
  // We can't read databases newer than we were designed for.
  if (meta_table_.GetCompatibleVersionNumber() > kCurrentVersionNumber) {
    LOG(WARNING) << "History database is too new.";
    return INIT_TOO_NEW;
  }

  // NOTICE: If you are changing structures for things shared with the archived
  // history file like URLs, visits, or downloads, that will need migration as
  // well. Instead of putting such migration code in this class, it should be
  // in the corresponding file (url_database.cc, etc.) and called from here and
  // from the archived_database.cc.

  int cur_version = meta_table_.GetVersionNumber();

  // Put migration code here

  if (cur_version == 15) {
    if (!MigrateBookmarksToFile(tmp_bookmarks_path) ||
        !DropStarredIDFromURLs()) {
      LOG(WARNING) << "Unable to update history database to version 16.";
      return INIT_FAILURE;
    }
    ++cur_version;
    meta_table_.SetVersionNumber(cur_version);
    meta_table_.SetCompatibleVersionNumber(
        std::min(cur_version, kCompatibleVersionNumber));
  }

  // When the version is too old, we just try to continue anyway, there should
  // not be a released product that makes a database too old for us to handle.
  LOG_IF(WARNING, cur_version < kCurrentVersionNumber) <<
      "History database version " << cur_version << " is too old to handle.";

  return INIT_OK;
}

}  // namespace history
