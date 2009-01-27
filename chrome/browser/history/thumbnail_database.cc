// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/history/thumbnail_database.h"

#include "base/file_util.h"
#include "base/time.h"
#include "base/string_util.h"
#include "chrome/browser/history/history_publisher.h"
#include "chrome/browser/history/url_database.h"
#include "chrome/common/jpeg_codec.h"
#include "chrome/common/sqlite_utils.h"
#include "chrome/common/thumbnail_score.h"
#include "skia/include/SkBitmap.h"

using base::Time;

namespace history {

// Version number of the database.
static const int kCurrentVersionNumber = 3;
static const int kCompatibleVersionNumber = 3;

ThumbnailDatabase::ThumbnailDatabase()
    : db_(NULL),
      statement_cache_(NULL),
      transaction_nesting_(0),
      history_publisher_(NULL) {
}

ThumbnailDatabase::~ThumbnailDatabase() {
  // The DBCloseScoper will delete the DB and the cache.
}

InitStatus ThumbnailDatabase::Init(const std::wstring& db_name,
                                   const HistoryPublisher* history_publisher) {
  history_publisher_ = history_publisher;

  // Open the thumbnail database, using the narrow version of open so that
  // the DB is in UTF-8.
  if (sqlite3_open(WideToUTF8(db_name).c_str(), &db_) != SQLITE_OK)
    return INIT_FAILURE;

  // Set the database page size to something  larger to give us
  // better performance (we're typically seek rather than bandwidth limited).
  // This only has an effect before any tables have been created, otherwise
  // this is a NOP. Must be a power of 2 and a max of 8192. We use a bigger
  // one because we're storing larger data (4-16K) in it, so we want a few
  // blocks per element.
  sqlite3_exec(db_, "PRAGMA page_size=4096", NULL, NULL, NULL);

  // The UI is generally designed to work well when the thumbnail database is
  // slow, so we can tolerate much less caching. The file is also very large
  // and so caching won't save a significant percentage of it for us,
  // reducing the benefit of caching in the first place. With the default cache
  // size of 2000 pages, it will take >8MB of memory, so reducing it can be a
  // big savings.
  sqlite3_exec(db_, "PRAGMA cache_size=64", NULL, NULL, NULL);

  // Run the database in exclusive mode. Nobody else should be accessing the
  // database while we're running, and this will give somewhat improved perf.
  sqlite3_exec(db_, "PRAGMA locking_mode=EXCLUSIVE", NULL, NULL, NULL);

  statement_cache_ = new SqliteStatementCache;
  DBCloseScoper scoper(&db_, &statement_cache_);

  // Scope initialization in a transaction so we can't be partially initialized.
  SQLTransaction transaction(db_);
  transaction.Begin();

  // Create the tables.
  if (!meta_table_.Init(std::string(), kCurrentVersionNumber,
                        kCompatibleVersionNumber, db_) ||
      !InitThumbnailTable() ||
      !InitFavIconsTable(false))
    return INIT_FAILURE;
  InitFavIconsIndex();

  // Version check. We should not encounter a database too old for us to handle
  // in the wild, so we try to continue in that case.
  if (meta_table_.GetCompatibleVersionNumber() > kCurrentVersionNumber) {
    LOG(WARNING) << "Thumbnail database is too new.";
    return INIT_TOO_NEW;
  }

  int cur_version = meta_table_.GetVersionNumber();
  if (cur_version == 2) {
    if (!UpgradeToVersion3()) {
      LOG(WARNING) << "Unable to update to thumbnail database to version 3.";
      return INIT_FAILURE;
    }
    ++cur_version;
  }

  LOG_IF(WARNING, cur_version < kCurrentVersionNumber) <<
      "Thumbnail database version " << cur_version << " is too old to handle.";

  // Initialization is complete.
  if (transaction.Commit() != SQLITE_OK)
    return INIT_FAILURE;

  // Initialize the statement cache and the scoper to automatically close it.
  statement_cache_->set_db(db_);
  scoper.Detach();
  close_scoper_.Attach(&db_, &statement_cache_);

  // The following code is useful in debugging the thumbnail database. Upon
  // startup, it will spit out a file for each thumbnail in the database so you
  // can open them in an external viewer. Insert the path name on your system
  // into the string below (I recommend using a blank directory since there may
  // be a lot of files).
#if 0
  SQLStatement statement;
  statement.prepare(db_, "SELECT id, image_data FROM favicons");
  while (statement.step() == SQLITE_ROW) {
    int idx = statement.column_int(0);
    std::vector<unsigned char> data;
    statement.column_blob_as_vector(1, &data);

    char filename[256];
    sprintf(filename, "<<< YOUR PATH HERE >>>\\%d.jpeg", idx);
    if (!data.empty()) {
      file_util::WriteFile(ASCIIToWide(std::string(filename)),
                           reinterpret_cast<char*>(data.data()),
                           data.size());
    }
  }
#endif

  return INIT_OK;
}

bool ThumbnailDatabase::InitThumbnailTable() {
  if (!DoesSqliteTableExist(db_, "thumbnails")) {
    if (sqlite3_exec(db_, "CREATE TABLE thumbnails ("
        "url_id INTEGER PRIMARY KEY,"
        "boring_score DOUBLE DEFAULT 1.0,"
        "good_clipping INTEGER DEFAULT 0,"
        "at_top INTEGER DEFAULT 0,"
        "last_updated INTEGER DEFAULT 0,"
        "data BLOB)", NULL, NULL, NULL) != SQLITE_OK)
      return false;
  }
  return true;
}

bool ThumbnailDatabase::UpgradeToVersion3() {
  // sqlite doesn't like the "ALTER TABLE xxx ADD (column_one, two,
  // three)" syntax, so list out the commands we need to execute:
  const char* alterations[] = {
    "ALTER TABLE thumbnails ADD boring_score DOUBLE DEFAULT 1.0",
    "ALTER TABLE thumbnails ADD good_clipping INTEGER DEFAULT 0",
    "ALTER TABLE thumbnails ADD at_top INTEGER DEFAULT 0",
    "ALTER TABLE thumbnails ADD last_updated INTEGER DEFAULT 0",
    NULL
  };

  for (int i = 0; alterations[i] != NULL; ++i) {
    if (sqlite3_exec(db_, alterations[i],
                     NULL, NULL, NULL) != SQLITE_OK) {
      NOTREACHED();
      return false;
    }
  }

  meta_table_.SetVersionNumber(3);
  meta_table_.SetCompatibleVersionNumber(std::min(3, kCompatibleVersionNumber));
  return true;
}

bool ThumbnailDatabase::RecreateThumbnailTable() {
  if (sqlite3_exec(db_, "DROP TABLE thumbnails", NULL, NULL, NULL) != SQLITE_OK)
    return false;
  return InitThumbnailTable();
}

bool ThumbnailDatabase::InitFavIconsTable(bool is_temporary) {
  // Note: if you update the schema, don't forget to update
  // CopyToTemporaryFaviconTable as well.
  const char* name = is_temporary ? "temp_favicons" : "favicons";
  if (!DoesSqliteTableExist(db_, name)) {
    std::string sql;
    sql.append("CREATE TABLE ");
    sql.append(name);
    sql.append("("
               "id INTEGER PRIMARY KEY,"
               "url LONGVARCHAR NOT NULL,"
               "last_updated INTEGER DEFAULT 0,"
               "image_data BLOB)");
    if (sqlite3_exec(db_, sql.c_str(), NULL, NULL, NULL) != SQLITE_OK)
      return false;
  }
  return true;
}

void ThumbnailDatabase::InitFavIconsIndex() {
  // Add an index on the url column. We ignore errors. Since this is always
  // called during startup, the index will normally already exist.
  sqlite3_exec(db_, "CREATE INDEX favicons_url ON favicons(url)",
               NULL, NULL, NULL);
}

void ThumbnailDatabase::BeginTransaction() {
  DCHECK(db_);
  if (transaction_nesting_ == 0) {
    int rv = sqlite3_exec(db_, "BEGIN TRANSACTION", NULL, NULL, NULL);
    DCHECK(rv == SQLITE_OK) << "Failed to begin transaction";
  }
  transaction_nesting_++;
}

void ThumbnailDatabase::CommitTransaction() {
  DCHECK(db_);
  DCHECK(transaction_nesting_ > 0) << "Committing too many transaction";
  transaction_nesting_--;
  if (transaction_nesting_ == 0) {
    int rv = sqlite3_exec(db_, "COMMIT", NULL, NULL, NULL);
    DCHECK(rv == SQLITE_OK) << "Failed to commit transaction";
  }
}

void ThumbnailDatabase::Vacuum() {
  DCHECK(transaction_nesting_ == 0) <<
      "Can not have a transaction when vacuuming.";
  sqlite3_exec(db_, "VACUUM", NULL, NULL, NULL);
}

void ThumbnailDatabase::SetPageThumbnail(
    const GURL& url,
    URLID id,
    const SkBitmap& thumbnail,
    const ThumbnailScore& score,
    const Time& time) {
  if (!thumbnail.isNull()) {
    bool add_thumbnail = true;
    ThumbnailScore current_score;
    if (ThumbnailScoreForId(id, &current_score)) {
      add_thumbnail = ShouldReplaceThumbnailWith(current_score, score);
    }

    if (add_thumbnail) {
      SQLITE_UNIQUE_STATEMENT(
          statement, *statement_cache_,
          "INSERT OR REPLACE INTO thumbnails "
          "(url_id, boring_score, good_clipping, at_top, last_updated, data) "
          "VALUES (?,?,?,?,?,?)");
      if (!statement.is_valid())
        return;

      // We use 90 quality (out of 100) which is pretty high, because
      // we're very sensitive to artifacts for these small sized,
      // highly detailed images.
      std::vector<unsigned char> jpeg_data;
      SkAutoLockPixels thumbnail_lock(thumbnail);
      bool encoded = JPEGCodec::Encode(
          reinterpret_cast<unsigned char*>(thumbnail.getAddr32(0, 0)),
          JPEGCodec::FORMAT_BGRA, thumbnail.width(),
          thumbnail.height(),
          static_cast<int>(thumbnail.rowBytes()), 90,
          &jpeg_data);

      if (encoded) {
        statement->bind_int64(0, id);
        statement->bind_double(1, score.boring_score);
        statement->bind_bool(2, score.good_clipping);
        statement->bind_bool(3, score.at_top);
        statement->bind_int64(4, score.time_at_snapshot.ToTimeT());
        statement->bind_blob(5, &jpeg_data[0],
                             static_cast<int>(jpeg_data.size()));
        if (statement->step() != SQLITE_DONE)
          DLOG(WARNING) << "Unable to insert thumbnail";
      }

      // Publish the thumbnail to any indexers listening to us.
      // The tests may send an invalid url. Hence avoid publishing those.
      if (url.is_valid() && history_publisher_ != NULL)
        history_publisher_->PublishPageThumbnail(jpeg_data, url, time);
    }
  } else {
    if ( !DeleteThumbnail(id) )
      DLOG(WARNING) << "Unable to delete thumbnail";
  }
}

bool ThumbnailDatabase::GetPageThumbnail(URLID id,
                                         std::vector<unsigned char>* data) {
  SQLITE_UNIQUE_STATEMENT(
      statement, *statement_cache_,
      "SELECT data FROM thumbnails WHERE url_id=?");
  if (!statement.is_valid())
    return false;

  statement->bind_int64(0, id);
  if (statement->step() != SQLITE_ROW)
    return false;  // don't have a thumbnail for this ID

  return statement->column_blob_as_vector(0, data);
}

bool ThumbnailDatabase::DeleteThumbnail(URLID id) {
  SQLITE_UNIQUE_STATEMENT(
      statement, *statement_cache_,
      "DELETE FROM thumbnails WHERE url_id = ?");
  if (!statement.is_valid())
    return false;

  statement->bind_int64(0, id);
  return statement->step() == SQLITE_DONE;
}

bool ThumbnailDatabase::ThumbnailScoreForId(
    URLID id,
    ThumbnailScore* score) {
  // Fetch the current thumbnail's information to make sure we
  // aren't replacing a good thumbnail with one that's worse.
  SQLITE_UNIQUE_STATEMENT(
      select_statement, *statement_cache_,
      "SELECT boring_score, good_clipping, at_top, last_updated "
      "FROM thumbnails WHERE url_id=?");
  if (!select_statement.is_valid()) {
    NOTREACHED() << "Couldn't build select statement!";
  } else {
    select_statement->bind_int64(0, id);
    if (select_statement->step() == SQLITE_ROW) {
      double current_boring_score = select_statement->column_double(0);
      bool current_clipping = select_statement->column_bool(1);
      bool current_at_top = select_statement->column_bool(2);
      Time last_updated = Time::FromTimeT(select_statement->column_int64(3));
      *score = ThumbnailScore(current_boring_score, current_clipping,
                              current_at_top, last_updated);
      return true;
    }
  }

  return false;
}

bool ThumbnailDatabase::SetFavIcon(URLID icon_id,
                                   const std::vector<unsigned char>& icon_data,
                                   Time time) {
  DCHECK(icon_id);
  if (icon_data.size()) {
    SQLITE_UNIQUE_STATEMENT(
        statement, *statement_cache_,
        "UPDATE favicons SET image_data=?, last_updated=? WHERE id=?");
    if (!statement.is_valid())
      return 0;

    statement->bind_blob(0, &icon_data.front(),
                        static_cast<int>(icon_data.size()));
    statement->bind_int64(1, time.ToTimeT());
    statement->bind_int64(2, icon_id);
    return statement->step() == SQLITE_DONE;
  } else {
    SQLITE_UNIQUE_STATEMENT(
        statement, *statement_cache_,
        "UPDATE favicons SET image_data=NULL, last_updated=? WHERE id=?");
    if (!statement.is_valid())
      return 0;

    statement->bind_int64(0, time.ToTimeT());
    statement->bind_int64(1, icon_id);
    return statement->step() == SQLITE_DONE;
  }
}

bool ThumbnailDatabase::SetFavIconLastUpdateTime(FavIconID icon_id,
                                                 const Time& time) {
  SQLITE_UNIQUE_STATEMENT(
      statement, *statement_cache_,
      "UPDATE favicons SET last_updated=? WHERE id=?");
  if (!statement.is_valid())
    return 0;

  statement->bind_int64(0, time.ToTimeT());
  statement->bind_int64(1, icon_id);
  return statement->step() == SQLITE_DONE;
}

FavIconID ThumbnailDatabase::GetFavIconIDForFavIconURL(const GURL& icon_url) {
  SQLITE_UNIQUE_STATEMENT(statement, *statement_cache_,
                          "SELECT id FROM favicons WHERE url=?");
  if (!statement.is_valid())
    return 0;

  statement->bind_string(0, URLDatabase::GURLToDatabaseURL(icon_url));
  if (statement->step() != SQLITE_ROW)
    return 0;  // not cached

  return statement->column_int64(0);
}

bool ThumbnailDatabase::GetFavIcon(
    FavIconID icon_id,
    Time* last_updated,
    std::vector<unsigned char>* png_icon_data,
    GURL* icon_url) {
  DCHECK(icon_id);

  SQLITE_UNIQUE_STATEMENT(statement, *statement_cache_,
      "SELECT last_updated, image_data, url FROM favicons WHERE id=?");
  if (!statement.is_valid())
    return 0;

  statement->bind_int64(0, icon_id);

  if (statement->step() != SQLITE_ROW) {
    // No entry for the id.
    return false;
  }

  *last_updated = Time::FromTimeT(statement->column_int64(0));
  if (statement->column_bytes(1) > 0)
    statement->column_blob_as_vector(1, png_icon_data);
  if (icon_url)
    *icon_url = GURL(statement->column_string(2));

  return true;
}

FavIconID ThumbnailDatabase::AddFavIcon(const GURL& icon_url) {
  SQLITE_UNIQUE_STATEMENT(statement, *statement_cache_,
                          "INSERT INTO favicons (url) VALUES (?)");
  if (!statement.is_valid())
    return 0;

  statement->bind_string(0, URLDatabase::GURLToDatabaseURL(icon_url));
  if (statement->step() != SQLITE_DONE)
    return 0;
  return sqlite3_last_insert_rowid(db_);
}

bool ThumbnailDatabase::DeleteFavIcon(FavIconID id) {
  SQLITE_UNIQUE_STATEMENT(statement, *statement_cache_,
                          "DELETE FROM favicons WHERE id = ?");
  if (!statement.is_valid())
    return false;
  statement->bind_int64(0, id);
  return statement->step() == SQLITE_DONE;
}

FavIconID ThumbnailDatabase::CopyToTemporaryFavIconTable(FavIconID source) {
  SQLITE_UNIQUE_STATEMENT(statement, *statement_cache_,
      "INSERT INTO temp_favicons (url, last_updated, image_data)"
      "SELECT url, last_updated, image_data "
      "FROM favicons WHERE id = ?");
  if (!statement.is_valid())
    return 0;
  statement->bind_int64(0, source);
  if (statement->step() != SQLITE_DONE)
    return 0;

  // We return the ID of the newly inserted favicon.
  return sqlite3_last_insert_rowid(db_);
}

bool ThumbnailDatabase::CommitTemporaryFavIconTable() {
  // Delete the old favicons table.
  if (sqlite3_exec(db_, "DROP TABLE favicons", NULL, NULL, NULL) != SQLITE_OK)
    return false;

  // Rename the temporary one.
  if (sqlite3_exec(db_, "ALTER TABLE temp_favicons RENAME TO favicons",
                   NULL, NULL, NULL) != SQLITE_OK)
    return false;

  // The renamed table needs the index (the temporary table doesn't have one).
  InitFavIconsIndex();
  return true;
}

}  // namespace history

