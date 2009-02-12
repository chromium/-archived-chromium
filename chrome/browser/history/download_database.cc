// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/history/download_database.h"

#include <limits>
#include <vector>

#include "chrome/browser/download/download_manager.h"
#include "chrome/browser/history/download_types.h"
#include "chrome/common/sqlite_utils.h"
#include "chrome/common/sqlite_compiled_statement.h"

using base::Time;

// Download schema:
//
//   id             SQLite-generated primary key.
//   full_path      Location of the download on disk.
//   url            URL of the downloaded file.
//   start_time     When the download was started.
//   received_bytes Total size downloaded.
//   total_bytes    Total size of the download.
//   state          Identifies if this download is completed or not. Not used
//                  directly by the history system. See DownloadItem's
//                  DownloadState for where this is used.

namespace history {

DownloadDatabase::DownloadDatabase() {
}

DownloadDatabase::~DownloadDatabase() {
}

bool DownloadDatabase::InitDownloadTable() {
  if (!DoesSqliteTableExist(GetDB(), "downloads")) {
    if (sqlite3_exec(GetDB(),
                     "CREATE TABLE downloads ("
                     "id INTEGER PRIMARY KEY,"
                     "full_path LONGVARCHAR NOT NULL,"
                     "url LONGVARCHAR NOT NULL,"
                     "start_time INTEGER NOT NULL,"
                     "received_bytes INTEGER NOT NULL,"
                     "total_bytes INTEGER NOT NULL,"
                     "state INTEGER NOT NULL)", NULL, NULL, NULL) != SQLITE_OK)
      return false;
  }
  return true;
}

bool DownloadDatabase::DropDownloadTable() {
  return sqlite3_exec(GetDB(), "DROP TABLE downloads", NULL, NULL, NULL) ==
      SQLITE_OK;
}

void DownloadDatabase::QueryDownloads(std::vector<DownloadCreateInfo>* results) {
  results->clear();

  SQLITE_UNIQUE_STATEMENT(statement, GetStatementCache(),
      "SELECT id, full_path, url, start_time, received_bytes, "
        "total_bytes, state "
      "FROM downloads "
      "ORDER BY start_time");
  if (!statement.is_valid())
    return;

  while (statement->step() == SQLITE_ROW) {
    DownloadCreateInfo info;
    info.db_handle = statement->column_int64(0);
    std::wstring path_str;
    statement->column_wstring(1, &path_str);
    info.path = FilePath::FromWStringHack(path_str);
    std::wstring url_str;
    statement->column_wstring(2, &url_str);
    info.url = GURL(WideToUTF8(url_str));
    info.start_time = Time::FromTimeT(statement->column_int64(3));
    info.received_bytes = statement->column_int64(4);
    info.total_bytes = statement->column_int64(5);
    info.state = statement->column_int(6);
    results->push_back(info);
  }
}

bool DownloadDatabase::UpdateDownload(int64 received_bytes,
                                      int32 state,
                                      DownloadID db_handle) {
  DCHECK(db_handle > 0);
  SQLITE_UNIQUE_STATEMENT(statement, GetStatementCache(),
      "UPDATE downloads "
      "SET received_bytes=?, state=? WHERE id=?");
  if (!statement.is_valid())
    return false;

  statement->bind_int64(0, received_bytes);
  statement->bind_int(1, state);
  statement->bind_int64(2, db_handle);
  return statement->step() == SQLITE_DONE;
}

bool DownloadDatabase::UpdateDownloadPath(const std::wstring& path,
                                          DownloadID db_handle) {
  DCHECK(db_handle > 0);
  SQLITE_UNIQUE_STATEMENT(statement, GetStatementCache(),
      "UPDATE downloads "
      "SET full_path=? WHERE id=?");
  if (!statement.is_valid())
    return false;

  statement->bind_wstring(0, path);
  statement->bind_int64(1, db_handle);
  return statement->step() == SQLITE_DONE;
}

int64 DownloadDatabase::CreateDownload(const DownloadCreateInfo& info) {
  SQLITE_UNIQUE_STATEMENT(statement, GetStatementCache(),
      "INSERT INTO downloads "
      "(full_path, url, start_time, received_bytes, total_bytes, state) "
      "VALUES (?, ?, ?, ?, ?, ?)");
  if (!statement.is_valid())
    return 0;

  statement->bind_wstring(0, info.path.ToWStringHack());
  statement->bind_wstring(1, UTF8ToWide(info.url.spec()));
  statement->bind_int64(2, info.start_time.ToTimeT());
  statement->bind_int64(3, info.received_bytes);
  statement->bind_int64(4, info.total_bytes);
  statement->bind_int(5, info.state);
  if (statement->step() == SQLITE_DONE)
    return sqlite3_last_insert_rowid(GetDB());

  return 0;
}

void DownloadDatabase::RemoveDownload(DownloadID db_handle) {
  SQLITE_UNIQUE_STATEMENT(statement, GetStatementCache(),
      "DELETE FROM downloads WHERE id=?");
  if (!statement.is_valid())
    return;

  statement->bind_int64(0, db_handle);
  statement->step();
}

void DownloadDatabase::RemoveDownloadsBetween(Time delete_begin,
                                              Time delete_end) {
  // This does not use an index. We currently aren't likely to have enough
  // downloads where an index by time will give us a lot of benefit.
  SQLITE_UNIQUE_STATEMENT(statement, GetStatementCache(),
      "DELETE FROM downloads WHERE start_time >= ? AND start_time < ? "
      "AND (State = ? OR State = ?)");
  if (!statement.is_valid())
    return;

  time_t start_time = delete_begin.ToTimeT();
  time_t end_time = delete_end.ToTimeT();
  statement->bind_int64(0, start_time);
  statement->bind_int64(1, end_time ? end_time : std::numeric_limits<int64>::max());
  statement->bind_int(2, DownloadItem::COMPLETE);
  statement->bind_int(3, DownloadItem::CANCELLED);
  statement->step();
}

void DownloadDatabase::SearchDownloads(std::vector<int64>* results,
                                       const std::wstring& search_text) {
  SQLITE_UNIQUE_STATEMENT(statement, GetStatementCache(),
      "SELECT id FROM downloads WHERE url LIKE ? "
      "OR full_path LIKE ? ORDER BY id");
  if (!statement.is_valid())
    return;

  std::wstring text(L"%");
  text.append(search_text);
  text.append(L"%");
  statement->bind_wstring(0, text);
  statement->bind_wstring(1, text);

  while (statement->step() == SQLITE_ROW)
    results->push_back(statement->column_int64(0));
}

}  // namespace history
