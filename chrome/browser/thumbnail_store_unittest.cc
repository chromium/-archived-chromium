// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string.h>
#include <algorithm>
#include <iostream>
#include <vector>

#include "chrome/browser/thumbnail_store.h"

#include "base/time.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/gfx/jpeg_codec.h"
#include "base/path_service.h"
#include "base/ref_counted.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/ref_counted_util.h"
#include "chrome/common/thumbnail_score.h"
#include "chrome/common/sqlite_compiled_statement.h"
#include "chrome/common/sqlite_utils.h"
#include "chrome/tools/profiles/thumbnail-inl.h"
#include "googleurl/src/gurl.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkPixelRef.h"

inline unsigned int diff(unsigned int a, unsigned int b) {
  return a>b ? a-b : b-a;
}

class ThumbnailStoreTest : public testing::Test {
 public:
  ThumbnailStoreTest() : score_(.5, true, false),
                         url_("http://www.google.com/") {
  }

  ~ThumbnailStoreTest() {
  }

 protected:
  void SetUp();

  void TearDown() {
    file_util::Delete(db_name_, false);
  }

  // Compute the max difference over all pixels for each RGBA component.
  void PrintPixelDiff(SkBitmap* image_a, SkBitmap* image_b);

  // The directory where ThumbnailStore will store data.
  FilePath db_name_;

  scoped_refptr<ThumbnailStore> store_;
  scoped_ptr<SkBitmap> google_;
  scoped_ptr<SkBitmap> weewar_;
  scoped_refptr<RefCountedBytes> jpeg_google_;
  scoped_refptr<RefCountedBytes> jpeg_weewar_;
  ThumbnailScore score_;
  GURL url_;
  base::Time time_;
};

void ThumbnailStoreTest::SetUp() {
  if (!file_util::GetTempDir(&db_name_))
    FAIL();

  // Delete any old thumbnail files if they exist.
  db_name_ = db_name_.AppendASCII("ThumbnailDB");
  file_util::Delete(db_name_, false);

  google_.reset(JPEGCodec::Decode(kGoogleThumbnail, sizeof(kGoogleThumbnail)));
  weewar_.reset(JPEGCodec::Decode(kWeewarThumbnail, sizeof(kWeewarThumbnail)));

  SkAutoLockPixels lock1(*google_);
  jpeg_google_ = new RefCountedBytes;
  JPEGCodec::Encode(
      reinterpret_cast<unsigned char*>(google_->getAddr32(0, 0)),
      JPEGCodec::FORMAT_BGRA, google_->width(),
      google_->height(),
      static_cast<int>(google_->rowBytes()), 90,
      &(jpeg_google_->data));

  SkAutoLockPixels lock2(*weewar_);
  jpeg_weewar_ = new RefCountedBytes;
  JPEGCodec::Encode(
      reinterpret_cast<unsigned char*>(weewar_->getAddr32(0,0)),
      JPEGCodec::FORMAT_BGRA, weewar_->width(),
      weewar_->height(),
      static_cast<int>(weewar_->rowBytes()), 90,
      &(jpeg_weewar_->data));

  store_ = new ThumbnailStore;

  store_->cache_.reset(new ThumbnailStore::Cache);
  store_->redirect_urls_.reset(new history::RedirectMap);

  store_->most_visited_urls_.reset(new std::vector<GURL>);
  store_->most_visited_urls_->push_back(url_);
}

void ThumbnailStoreTest::PrintPixelDiff(SkBitmap* image_a, SkBitmap* image_b) {
  // Compute the maximum difference in each of the RGBA components across all
  // pixels between the retrieved SkBitmap and the original.  These
  // differences should be small since encoding was done at 90% before
  // writing to disk.

  if (image_a->height() != image_b->height() ||
      image_b->width() != image_b->width() ||
      image_a->rowBytes() != image_b->rowBytes())
    return;

  SkAutoLockPixels lock_a(*image_a);
  SkAutoLockPixels lock_b(*image_b);

  int ppr = image_a->rowBytesAsPixels();
  unsigned int *a, *b;
  unsigned int maxv[4];
  memset(maxv, 0, sizeof(maxv));

  for (int nrows = image_a->height()-1; nrows >= 0; nrows--) {
    a = image_a->getAddr32(0, nrows);
    b = image_b->getAddr32(0, nrows);
    for (int i = 0; i < ppr; i += 4) {
      for (int j = 0; j < 4; j++) {
        maxv[j] = std::max(diff(*(a+i) >> (j<<3) & 0xff,
                                *(b+i) >> (j<<3) & 0xff),
                           maxv[j]);
      }
    }
  }

  std::cout << "Max diff btwn original and encoded image (b,g,r,a) = ("
            << maxv[0] << ","
            << maxv[1] << ","
            << maxv[2] << ","
            << maxv[3] << ")" << std::endl;
}

TEST_F(ThumbnailStoreTest, UpdateThumbnail) {
  RefCountedBytes* read_image = NULL;
  ThumbnailScore score2(0.1, true, true);

  // store_ google_ with a low score, then weewar_ with a higher score
  // and check that weewar_ overwrote google_.

  EXPECT_TRUE(store_->SetPageThumbnail(url_, *google_, score_));
  EXPECT_TRUE(store_->SetPageThumbnail(url_, *weewar_, score2));

  EXPECT_TRUE(store_->GetPageThumbnail(url_, &read_image));
  EXPECT_EQ(read_image->data.size(), jpeg_weewar_->data.size());
  EXPECT_EQ(0, memcmp(&read_image->data[0], &jpeg_weewar_->data[0],
                      jpeg_weewar_->data.size()));

  read_image->Release();
}

TEST_F(ThumbnailStoreTest, RetrieveFromCache) {
  RefCountedBytes* read_image = NULL;

  // Retrieve a thumbnail/score for a page not in the cache.

  EXPECT_FALSE(store_->GetPageThumbnail(GURL("nonexistent"), &read_image));

  // Store a thumbnail into the cache and retrieve it.

  EXPECT_TRUE(store_->SetPageThumbnail(url_, *google_, score_));
  EXPECT_TRUE(store_->GetPageThumbnail(url_, &read_image));
  EXPECT_TRUE(score_.Equals((*store_->cache_)[url_].score_));
  EXPECT_TRUE(read_image->data.size() == jpeg_google_->data.size());
  EXPECT_EQ(0, memcmp(&read_image->data[0], &jpeg_google_->data[0],
                      jpeg_google_->data.size()));

  read_image->Release();
}

TEST_F(ThumbnailStoreTest, RetrieveFromDisk) {
  EXPECT_TRUE(store_->SetPageThumbnail(url_, *google_, score_));

  // Write the thumbnail to disk and retrieve it.

  store_->InitializeFromDB(db_name_, NULL);
  store_->CommitCacheToDB(NULL);  // Write to the DB (dirty bit sould be set)
  store_->cache_->clear();        // Clear it from the cache.

  // Read from the DB.
  SQLITE_UNIQUE_STATEMENT(statement, *store_->statement_cache_,
      "SELECT * FROM thumbnails");
  EXPECT_TRUE(statement->step() == SQLITE_ROW);
  GURL url(statement->column_string(0));
  ThumbnailScore score(statement->column_double(1),
                       statement->column_bool(2),
                       statement->column_bool(3),
                       base::Time::FromInternalValue(
                          statement->column_int64(4)));
  scoped_refptr<RefCountedBytes> data = new RefCountedBytes;
  EXPECT_TRUE(statement->column_blob_as_vector(5, &data->data));

  EXPECT_TRUE(url == url_);
  EXPECT_TRUE(score.Equals(score_));
  EXPECT_TRUE(data->data.size() == jpeg_google_->data.size());
  EXPECT_EQ(0, memcmp(&data->data[0], &jpeg_google_->data[0],
                      jpeg_google_->data.size()));
}

TEST_F(ThumbnailStoreTest, FollowRedirects) {
  RefCountedBytes* read_image = NULL;
  std::vector<GURL> redirects;

  GURL my_url("google");
  redirects.push_back(GURL("google.com"));
  redirects.push_back(GURL("www.google.com"));
  redirects.push_back(url_);  // url_ = http://www.google.com/
  (*store_->redirect_urls_)[my_url] = new RefCountedVector<GURL>(redirects);

  store_->most_visited_urls_->push_back(my_url);

  EXPECT_TRUE(store_->SetPageThumbnail(GURL("google.com"), *google_, score_));
  EXPECT_TRUE(store_->GetPageThumbnail(my_url, &read_image));

  read_image->Release();
}
