// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_HISTORY_DOWNLOAD_DATABASE_H__
#define CHROME_BROWSER_HISTORY_DOWNLOAD_DATABASE_H__

#include "chrome/browser/history/history_types.h"

struct sqlite3;
class SqliteStatementCache;
class SQLStatement;
struct DownloadCreateInfo;

namespace history {

// Maintains a table of downloads.
class DownloadDatabase {
 public:
  // Must call InitDownloadTable before using any other functions.
  DownloadDatabase();
  virtual ~DownloadDatabase();

  // Get all the downloads from the database.
  void QueryDownloads(std::vector<DownloadCreateInfo>* results);

  // Update the state of one download. Returns true if successful.
  bool UpdateDownload(int64 received_bytes, int32 state, DownloadID db_handle);

  // Update the path of one download. Returns true if successful.
  bool UpdateDownloadPath(const std::wstring& path, DownloadID db_handle);

  // Create a new database entry for one download and return its primary db id.
  int64 CreateDownload(const DownloadCreateInfo& info);

  // Remove a download from the database.
  void RemoveDownload(DownloadID db_handle);

  // Remove all completed downloads that started after |remove_begin|
  // (inclusive) and before |remove_end|. You may use null Time values
  // to do an unbounded delete in either direction. This function ignores
  // all downloads that are in progress or are waiting to be cancelled.
  void RemoveDownloadsBetween(base::Time remove_begin, base::Time remove_end);

  // Search for downloads matching the search text.
  void SearchDownloads(std::vector<int64>* results,
                       const std::wstring& search_text);

 protected:
  // Returns the database and statement cache for the functions in this
  // interface. The descendant of this class implements these functions to
  // return its objects.
  virtual sqlite3* GetDB() = 0;
  virtual SqliteStatementCache& GetStatementCache() = 0;

  // Creates the downloads table if needed.
  bool InitDownloadTable();

  // Used to quickly clear the downloads. First you would drop it, then you
  // would re-initialize it.
  bool DropDownloadTable();

 private:
  DISALLOW_EVIL_CONSTRUCTORS(DownloadDatabase);
};

}  // namespace history

#endif  // CHROME_BROWSER_HISTORY_DOWNLOAD_DATABASE_H__
