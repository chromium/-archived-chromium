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

  // Create a new database entry for one download and return its primary db id.
  int64 CreateDownload(const DownloadCreateInfo& info);

  // Remove a download from the database.
  void RemoveDownload(DownloadID db_handle);

  // Remove all completed downloads that started after |remove_begin|
  // (inclusive) and before |remove_end|. You may use null Time values
  // to do an unbounded delete in either direction. This function ignores
  // all downloads that are in progress or are waiting to be cancelled.
  void RemoveDownloadsBetween(Time remove_begin, Time remove_end);

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
