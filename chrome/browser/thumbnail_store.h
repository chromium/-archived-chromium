// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_THUMBNAIL_STORE_H_
#define CHROME_BROWSER_THUMBNAIL_STORE_H_

#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/file_path.h"
#include "base/message_loop.h"
#include "base/ref_counted.h"
#include "base/timer.h"
#include "chrome/browser/cancelable_request.h"
#include "chrome/browser/history/history.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/ref_counted_util.h"
#include "testing/gtest/include/gtest/gtest_prod.h"

class DictionaryValue;
class GURL;
class HistoryService;
class PageUsageData;
class Pickle;
class Profile;
class SkBitmap;
struct ThumbnailScore;
namespace base {
class Time;
}

// This storage interface provides storage for the thumbnails used
// by the new_tab_ui.
class ThumbnailStore : public base::RefCountedThreadSafe<ThumbnailStore> {
 public:
  typedef Callback2<int, scoped_refptr<RefCountedBytes> >::Type
      ThumbnailDataCallback;

  ThumbnailStore();
  ~ThumbnailStore();

  // Must be called after creation but before other methods are called.
  // file_path is a directory where a new database should be created
  // or the location of an existing databse.
  void Init(const FilePath& file_path, Profile* profile);

  // Stores the given thumbnail and score with the associated url in the cache.
  // If write_to_disk is true, the thumbnail data is written to disk on the
  // file_thread.
  bool SetPageThumbnail(const GURL& url,
                        const SkBitmap& thumbnail,
                        const ThumbnailScore& score,
                        bool write_to_disk);

  // Sets *data to point to the thumbnail for the given url.
  // A return value of ASYNC means you should call GetPageThumbnailAsync.
  // On a return value of SUCCESS, the refcount of the out parameter data
  // is incremented for the caller who takes ownership of that reference.
  enum GetStatus { SUCCESS, FAIL, ASYNC };
  ThumbnailStore::GetStatus GetPageThumbnail(const GURL& url,
                                             RefCountedBytes** data);

  // Retrieves the redirects list for the given url asynchronously.
  // Calls the callback with the request_id and thumbnail data if found.
  void GetPageThumbnailAsync(const GURL& url,
                             int request_id,
                             ThumbnailStore::ThumbnailDataCallback* cb);

  // Cancels the given set of request_id's which were issued from
  // GetPageThumbnailAsync.
  // This method is called from ~DOMUIThumbnailSource.  If a
  // DOMUIThumbnailSource requests a thumbnail but is destroyed before the
  // data is sent back, this method will cancel the request and delete the
  // callback.
  void CancelPendingRequests(const std::set<int>& pending_requests);

 private:
  FRIEND_TEST(ThumbnailStoreTest, RetrieveFromCache);
  FRIEND_TEST(ThumbnailStoreTest, RetrieveFromDisk);
  FRIEND_TEST(ThumbnailStoreTest, UpdateThumbnail);
  FRIEND_TEST(ThumbnailStoreTest, FollowRedirects);

  // Data structure used to store thumbnail data in memory.
  typedef std::map<GURL, std::pair<scoped_refptr<RefCountedBytes>,
      ThumbnailScore> > Cache;

  // Data structure used to cache the redirect lists for urls.
  typedef std::map<GURL, scoped_refptr<RefCountedVector<GURL> > > RedirectMap;

  // Data structure used to store request_id's and callbacks for
  // GetPageThumbnailAsync.
  typedef std::map<int, std::pair<ThumbnailStore::ThumbnailDataCallback*,
                                  HistoryService::Handle> > RequestMap;

  // Deletes thumbnail data from disk for the given list of urls.
  void DeleteThumbnails(
      scoped_refptr<RefCountedVector<GURL> > thumbnail_urls) const;

  // Read all thumbnail data from the specified FilePath into a Cache object.
  // Done on the file_thread and returns to OnDiskDataAvailable on the thread
  // owning the specified MessageLoop.
  void GetAllThumbnailsFromDisk(FilePath filepath, MessageLoop* cb_loop);

  // Read the thumbnail data from the given file and stores it in the
  // out parameters GURL, SkBitmap, and ThumbnailScore.
  bool GetPageThumbnailFromDisk(const FilePath& file,
                                GURL* url,
                                RefCountedBytes* data,
                                ThumbnailScore* score) const;

  // Once thumbnail data from the disk is available from the file_thread,
  // this function is invoked on the main thread.  It takes ownership of the
  // Cache* passed in and retains this Cache* for the lifetime of the object.
  void OnDiskDataAvailable(ThumbnailStore::Cache* cache);

  // Write thumbnail data to disk for a given url.
  bool WriteThumbnailToDisk(const GURL& url,
                            scoped_refptr<RefCountedBytes> data,
                            const ThumbnailScore& score) const;

  // Pack the given ThumbnailScore into the given Pickle.
  void PackScore(const ThumbnailScore& score, Pickle* packed) const;

  // Unpack a ThumbnailScore from a given Pickle and associated iterator.
  // Returns false is a ThumbnailScore could not be unpacked.
  bool UnpackScore(ThumbnailScore* score,
                   const Pickle& packed,
                   void*& iter) const;

  bool ShouldStoreThumbnailForURL(const GURL& url) const;

  bool IsURLBlacklisted(const GURL& url) const;

  std::wstring GetDictionaryKeyForURL(const std::string& url) const;

  // Returns true if url is in most_visited_urls_.
  bool IsPopular(const GURL& url) const;

  // The callback for GetPageThumbnailAsync.  Caches the redirect list
  // and calls the callback for the request asssociated with the url.
  void OnRedirectQueryComplete(HistoryService::Handle request_handle,
                               GURL url,
                               bool success,
                               history::RedirectList* redirects);

  // Called on a timer initiated in Init().  Calls the HistoryService to
  // update the list of most visited URLs.  The callback is
  // OnMostVisitedURLsAvailable.
  void UpdateMostVisited();

  // Updates the list of most visited URLs.  Then calls CleanCacheData.
  void OnMostVisitedURLsAvailable(CancelableRequestProvider::Handle handle,
                                  std::vector<PageUsageData*>* data);

  // Remove entries from the in memory thumbnail cache and redirect lists
  // cache that have been blacklisted or are not in the top kMaxCacheSize
  // visited sites.
  void CleanCacheData();

  // The Cache maintained by the object.
  scoped_ptr<ThumbnailStore::Cache> cache_;
  bool cache_initialized_;

  // The location of the thumbnail store.
  FilePath file_path_;

  scoped_refptr<HistoryService> hs_;

  // A list of the most_visited_urls_ refreshed every 30mins from the
  // HistoryService.
  std::vector<GURL> most_visited_urls_;

  // A pointer to the persistent URL blacklist for this profile.
  const DictionaryValue* url_blacklist_;

  // A map pairing the URL that a user typed to a list of URLs it was
  // redirected to. Ex:
  // google.com => { http://www.google.com/ }
  ThumbnailStore::RedirectMap redirect_urls_;

  // When GetPageThumbnailAsync is called, this map records the request_id
  // and callback associated with the request.  When the thumbnail becomes
  // available, the callback is taken from this map and the thumbnail data
  // is returned to it.
  ThumbnailStore::RequestMap redirect_requests_;

  // Timer on which UpdateMostVisited runs.
  base::RepeatingTimer<ThumbnailStore> timer_;

  // Consumer for getting redirect lists from the history service.
  CancelableRequestConsumerT<int, -1> hs_consumer_;

  // Consumer for getting the list of most visited urls.
  CancelableRequestConsumerTSimple<PageUsageData*> cancelable_consumer_;

  static const int kMostVisitedScope = 90;
  static const unsigned int kMaxCacheSize = 45;

  DISALLOW_COPY_AND_ASSIGN(ThumbnailStore);
};

#endif  // CHROME_BROWSER_THUMBNAIL_STORE_H_
