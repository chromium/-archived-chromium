// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_HISTORY_THUMBNAIL_DATABASE_H_
#define CHROME_BROWSER_HISTORY_THUMBNAIL_DATABASE_H_

#include <vector>

#include "base/file_path.h"
#include "chrome/browser/history/history_types.h"
#include "chrome/browser/history/url_database.h"  // For DBCloseScoper.
#include "chrome/browser/meta_table_helper.h"
#include "chrome/common/sqlite_compiled_statement.h"
#include "skia/include/SkBitmap.h"

struct sqlite3;
struct ThumbnailScore;
namespace base {
  class Time;
}

namespace history {

class ExpireHistoryBackend;
class HistoryPublisher;

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
  InitStatus Init(const FilePath& db_name,
                  const HistoryPublisher* history_publisher);

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
  void SetPageThumbnail(const GURL& url,
                        URLID id,
                        const SkBitmap& thumbnail,
                        const ThumbnailScore& score,
                        const base::Time& time);

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
                  base::Time time);

  // Sets the time the favicon was last updated.
  bool SetFavIconLastUpdateTime(FavIconID icon_id, const base::Time& time);

  // Returns the id of the entry in the favicon database with the specified url.
  // Returns 0 if no entry exists for the specified url.
  FavIconID GetFavIconIDForFavIconURL(const GURL& icon_url);

  // Gets the png encoded favicon and last updated time for the specified
  // favicon id.
  bool GetFavIcon(FavIconID icon_id,
                  base::Time* last_updated,
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
  bool UpgradeToVersion3();

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

  // This object is created and managed by the history backend. We maintain an
  // opaque pointer to the object for our use.
  // This can be NULL if there are no indexers registered to receive indexing
  // data from us.
  const HistoryPublisher* history_publisher_;
};

}  // namespace history

#endif  // CHROME_BROWSER_HISTORY_THUMBNAIL_DATABASE_H_
