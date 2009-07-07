// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_THUMBNAIL_STORE_H_
#define CHROME_BROWSER_THUMBNAIL_STORE_H_

#include <map>
#include <string>
#include <vector>

#include "base/file_path.h"
#include "base/message_loop.h"
#include "base/ref_counted.h"
#include "base/timer.h"
#include "chrome/browser/cancelable_request.h"
#include "chrome/browser/history/history.h"
#include "chrome/browser/history/url_database.h"  // For DBCloseScoper
#include "chrome/common/pref_names.h"
#include "chrome/common/ref_counted_util.h"
#include "chrome/common/sqlite_compiled_statement.h"
#include "chrome/common/thumbnail_score.h"
#include "testing/gtest/include/gtest/gtest_prod.h"

class DictionaryValue;
class GURL;
class HistoryService;
class Profile;
class SkBitmap;
struct sqlite3;
namespace base {
class Time;
}

// This storage interface provides storage for the thumbnails used
// by the new_tab_ui.
class ThumbnailStore : public base::RefCountedThreadSafe<ThumbnailStore> {
 public:
  ThumbnailStore();
  ~ThumbnailStore();

  // Must be called after creation but before other methods are called.
  void Init(const FilePath& db_name,      // The location of the database.
            Profile* profile);            // To get to the HistoryService.

  // Stores the given thumbnail and score with the associated url in the cache.
  bool SetPageThumbnail(const GURL& url,
                        const SkBitmap& thumbnail,
                        const ThumbnailScore& score);

  // Sets *data to point to the thumbnail for the given url.
  // Returns false if no thumbnail available.
  bool GetPageThumbnail(const GURL& url, RefCountedBytes** data);

 private:
  FRIEND_TEST(ThumbnailStoreTest, RetrieveFromCache);
  FRIEND_TEST(ThumbnailStoreTest, RetrieveFromDisk);
  FRIEND_TEST(ThumbnailStoreTest, UpdateThumbnail);
  FRIEND_TEST(ThumbnailStoreTest, FollowRedirects);
  friend class ThumbnailStoreTest;

  struct CacheEntry {
    scoped_refptr<RefCountedBytes> data_;
    ThumbnailScore score_;
    bool dirty_;

    CacheEntry() : data_(NULL), score_(ThumbnailScore()), dirty_(false) {}
    CacheEntry(RefCountedBytes* data,
               const ThumbnailScore& score,
               bool dirty)
        : data_(data),
          score_(score),
          dirty_(dirty) {}
  };

  // Data structure used to store thumbnail data in memory.
  typedef std::map<GURL, CacheEntry> Cache;

  // Most visited URLs and their redirect lists -------------------------------

  // Query the HistoryService for the most visited URLs and the most recent
  // redirect lists for those URLs.  This happens in the background and the
  // callback is OnURLDataAvailable.
  void UpdateURLData();

  // The callback for UpdateURLData.
  void OnURLDataAvailable(std::vector<GURL>* urls,
                          history::RedirectMap* redirects);

  // Remove stale data --------------------------------------------------------

  // Remove entries from the in memory thumbnail cache and redirect lists
  // cache that have been blacklisted or are not in the top kMaxCacheSize
  // visited sites.
  void CleanCacheData();

  // Disk operations ----------------------------------------------------------

  // Initialize |db_| to the database specified in |db_name|.  If |cb_loop|
  // is non-null, calls GetAllThumbnailsFromDisk.  Done on the file_thread.
  void InitializeFromDB(const FilePath& db_name, MessageLoop* cb_loop);

  // Read all thumbnail data from the specified FilePath into a Cache object.
  // Done on the file_thread and returns to OnDiskDataAvailable on the thread
  // owning the specified MessageLoop.
  void GetAllThumbnailsFromDisk(MessageLoop* cb_loop);

  // Once thumbnail data from the disk is available from the file_thread,
  // this function is invoked on the main thread.  It takes ownership of the
  // Cache* passed in and retains this Cache* for the lifetime of the object.
  void OnDiskDataAvailable(Cache* cache);

  // Delete each URL in the given vector from the DB and write all dirty
  // cache entries to the DB.
  void CommitCacheToDB(
      scoped_refptr<RefCountedVector<GURL> > stale_urls) const;

  // Decide whether to store data ---------------------------------------------

  bool ShouldStoreThumbnailForURL(const GURL& url) const;

  bool IsURLBlacklisted(const GURL& url) const;

  std::wstring GetDictionaryKeyForURL(const std::string& url) const;

  // Returns true if url is in most_visited_urls_.
  bool IsPopular(const GURL& url) const;



  // Member variables ---------------------------------------------------------

  // The Cache maintained by the object.
  scoped_ptr<Cache> cache_;

  // The database holding the thumbnails on disk.
  sqlite3* db_;
  SqliteStatementCache* statement_cache_;
  history::DBCloseScoper close_scoper_;

  // We hold a reference to the history service to query for most visited URLs
  // and redirect information.
  scoped_refptr<HistoryService> hs_;

  // A list of the most_visited_urls_ refreshed every 30mins from the
  // HistoryService.
  scoped_ptr<std::vector<GURL> > most_visited_urls_;

  // A pointer to the persistent URL blacklist for this profile.
  const DictionaryValue* url_blacklist_;

  // A map pairing the URL that a user typed to a list of URLs it was
  // redirected to. Ex:
  // google.com => { http://www.google.com/ }
  scoped_ptr<history::RedirectMap> redirect_urls_;

  // Timer on which UpdateURLData runs.
  base::RepeatingTimer<ThumbnailStore> timer_;

  // Consumer for queries to the HistoryService.
  CancelableRequestConsumer consumer_;

  static const unsigned int kMaxCacheSize = 45;

  DISALLOW_COPY_AND_ASSIGN(ThumbnailStore);
};

#endif  // CHROME_BROWSER_THUMBNAIL_STORE_H_
