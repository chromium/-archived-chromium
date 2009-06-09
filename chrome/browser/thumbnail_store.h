// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_THUMBNAIL_STORE_H_
#define CHROME_BROWSER_THUMBNAIL_STORE_H_

#include <map>
#include <vector>

#include "base/file_path.h"
#include "base/message_loop.h"
#include "base/ref_counted.h"
#include "chrome/common/ref_counted_util.h"
#include "testing/gtest/include/gtest/gtest_prod.h"

class GURL;
class Pickle;
class SkBitmap;
struct ThumbnailScore;
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
  // file_path is a directory where a new database should be created
  // or the location of an existing databse.
  void Init(const FilePath& file_path);

  // Stores the given thumbnail and score with the associated url in the cache.
  // If write_to_disk is true, the thumbnail data is written to disk on the
  // file_thread.
  bool SetPageThumbnail(const GURL& url,
                        const SkBitmap& thumbnail,
                        const ThumbnailScore& score,
                        bool write_to_disk);

  // Sets *data to point to the thumbnail for the given url.
  // Returns false if there is not data for the given url or some other
  // error occurred.
  bool GetPageThumbnail(const GURL& url, RefCountedBytes** data);

 private:
  FRIEND_TEST(ThumbnailStoreTest, RetrieveFromCache);
  FRIEND_TEST(ThumbnailStoreTest, RetrieveFromDisk);

  // Data structure used to store thumbnail data in memory.
  typedef std::map<GURL, std::pair<scoped_refptr<RefCountedBytes>,
                                   ThumbnailScore> > Cache;

  // The location of the thumbnail store.
  FilePath file_path_;

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
  bool WriteThumbnailToDisk(const GURL& url) const;

  // Pack the given ThumbnailScore into the given Pickle.
  void PackScore(const ThumbnailScore& score, Pickle* packed) const;

  // Unpack a ThumbnailScore from a given Pickle and associated iterator.
  // Returns false is a ThumbnailScore could not be unpacked.
  bool UnpackScore(ThumbnailScore* score, const Pickle& packed,
                   void*& iter) const;

  // The Cache maintained by the object.
  scoped_ptr<ThumbnailStore::Cache> cache_;
  bool cache_initialized_;

  DISALLOW_COPY_AND_ASSIGN(ThumbnailStore);
};

#endif  // CHROME_BROWSER_THUMBNAIL_STORE_H_
