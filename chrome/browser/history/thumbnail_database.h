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

#ifndef CHROME_BROWSER_HISTORY_THUMBNAIL_DATABASE_H__
#define CHROME_BROWSER_HISTORY_THUMBNAIL_DATABASE_H__

#include <vector>

#include "chrome/browser/history/history_database.h"
#include "chrome/browser/history/history_types.h"
#include "chrome/browser/history/url_database.h"  // For DBCloseScoper.
#include "chrome/common/sqlite_compiled_statement.h"

struct sqlite3;
struct ThumbnailScore;
class Time;

namespace history {

class ExpireHistoryBackend;

// This database interface is owned by the history backend and runs on the
// history thread. It is a totally separate component from history partially
// because we may want to move it to its own thread in the future. The
// operations we will do on this database will be slow, but we can tolerate
// higher latency (it's OK for thumbnails to come in slower than the rest
// of the data). Moving this to a separate thread would not block potentially
// higher priority history operations.
class ThumbnailDatabase {
 public:
  ThumbnailDatabase();
  ~ThumbnailDatabase();

  // Must be called after creation but before any other methods are called.
  // When not INIT_OK, no other functions should be called.
  InitStatus Init(const std::wstring& db_name);

  // Transactions on the database.
  void BeginTransaction();
  void CommitTransaction();
  int transaction_nesting() const {
    return transaction_nesting_;
  }

  // Vacuums the database. This will cause sqlite to defragment and collect
  // unused space in the file. It can be VERY SLOW.
  void Vacuum();

  // Thumbnails ----------------------------------------------------------------

  // Sets the given data to be the thumbnail for the given URL,
  // overwriting any previous data. If the SkBitmap contains no pixel
  // data, the thumbnail will be deleted.
  void SetPageThumbnail(URLID id,
                        const SkBitmap& thumbnail,
                        const ThumbnailScore& score);

  // Retrieves thumbnail data for the given URL, returning true on success,
  // false if there is no such thumbnail or there was some other error.
  bool GetPageThumbnail(URLID id, std::vector<unsigned char>* data);

  // Delete the thumbnail with the provided id. Returns false on failure
  bool DeleteThumbnail(URLID id);

  // If there is a thumbnail score for the id provided, retrieves the
  // current thumbnail score and places it in |score| and returns
  // true. Returns false otherwise.
  bool ThumbnailScoreForId(URLID id, ThumbnailScore* score);

  // Called by the to delete all old thumbnails and make a clean table.
  // Returns true on success.
  bool RecreateThumbnailTable();

  // FavIcons ------------------------------------------------------------------

  // Sets the bits for a favicon. This should be png encoded data.
  // The time indicates the access time, and is used to detect when the favicon
  // should be refreshed.
  bool SetFavIcon(FavIconID icon_id,
                  const std::vector<unsigned char>& icon_data,
                  Time time);

  // Sets the time the favicon was last updated.
  bool SetFavIconLastUpdateTime(FavIconID icon_id, const Time& time);

  // Returns the id of the entry in the favicon database with the specified url.
  // Returns 0 if no entry exists for the specified url.
  FavIconID GetFavIconIDForFavIconURL(const GURL& icon_url);

  // Gets the png encoded favicon and last updated time for the specified
  // favicon id.
  bool GetFavIcon(FavIconID icon_id,
                  Time* last_updated,
                  std::vector<unsigned char>* png_icon_data,
                  GURL* icon_url);

  // Adds the favicon URL to the favicon db, returning its id.
  FavIconID AddFavIcon(const GURL& icon_url);

  // Delete the favicon with the provided id. Returns false on failure
  bool DeleteFavIcon(FavIconID id);

  // Temporary FavIcons --------------------------------------------------------

  // Create a temporary table to store favicons. Favicons will be copied to
  // this table by CopyToTemporaryFavIconTable() and then the original table
  // will be dropped, leaving only those copied favicons remaining. This is
  // used to quickly delete most of the favicons when clearing history.
  bool InitTemporaryFavIconsTable() {
    return InitFavIconsTable(true);
  }

  // Copies the given favicon from the "main" favicon table to the temporary
  // one. This is only valid in between calls to InitTemporaryFavIconsTable()
  // and CommitTemporaryFavIconTable().
  //
  // The ID of the favicon will change when this copy takes place. The new ID
  // is returned, or 0 on failure.
  FavIconID CopyToTemporaryFavIconTable(FavIconID source);

  // Replaces the main URL table with the temporary table created by
  // InitTemporaryFavIconsTable(). This will mean all favicons not copied over
  // will be deleted. Returns true on success.
  bool CommitTemporaryFavIconTable();

 private:
  friend class ExpireHistoryBackend;

  // Creates the thumbnail table, returning true if the table already exists
  // or was successfully created.
  bool InitThumbnailTable();

  // Creates the favicon table, returning true if the table already exists,
  // or was successfully created. is_temporary will be false when generating
  // the "regular" favicons table. The expirer sets this to true to generate the
  // temporary table, which will have a different name but the same schema.
  bool InitFavIconsTable(bool is_temporary);

  // Adds support for the new metadata on web page thumbnails.
  void UpgradeToVersion3();

  // Creates the index over the favicon table. This will be called during
  // initialization after the table is created. This is a separate function
  // because it is used by SwapFaviconTables to create an index over the
  // newly-renamed favicons table (formerly the temporary table with no index).
  void InitFavIconsIndex();

  // Ensures that db_ and statement_cache_ are destroyed in the proper order.
  DBCloseScoper close_scoper_;

  // The database connection and the statement cache: MAY BE NULL if database
  // init failed. These are cleaned up by the close_scoper_.
  sqlite3* db_;
  SqliteStatementCache* statement_cache_;

  int transaction_nesting_;

  MetaTableHelper meta_table_;
};

}  // namespace history

#endif  // CHROME_BROWSER_HISTORY_THUMBNAIL_DATABASE_H__
