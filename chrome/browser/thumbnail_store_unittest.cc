// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string.h>
#include <iostream>

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
  ThumbnailStoreTest() : score1_(.5, true, false),
    url1_("http://www.google.com"), url2_("http://www.elgoog.com") {
  }
  ~ThumbnailStoreTest() {
  }

 protected:
  void SetUp();

  void TearDown() {
    file_util::Delete(file_path_.AppendASCII(url1_.host()), false);
    file_util::Delete(file_path_.AppendASCII(url2_.host()), false);
  }

  // Compute the max difference over all pixels for each RGBA component.
  void PrintPixelDiff(SkBitmap* image_a, SkBitmap* image_b);

  // The directory where ThumbnailStore will store data.
  FilePath file_path_;

  SkBitmap image_;
  scoped_refptr<RefCountedBytes> jpeg_image_;
  ThumbnailScore score1_, score2_;
  GURL url1_, url2_;
  base::Time time_;
};

void ThumbnailStoreTest::SetUp() {
  if (!file_util::GetTempDir(&file_path_))
    FAIL();

  // Delete any old thumbnail files if they exist.
  file_util::Delete(file_path_.AppendASCII(url1_.host()), false);
  file_util::Delete(file_path_.AppendASCII(url2_.host()), false);

  // image is the original SkBitmap representing the thumbnail
  image_ = *(JPEGCodec::Decode(kGoogleThumbnail, sizeof(kGoogleThumbnail)));

  SkAutoLockPixels thumbnail_lock(image_);
  jpeg_image_ = new RefCountedBytes;
  JPEGCodec::Encode(
      reinterpret_cast<unsigned char*>(image_.getAddr32(0, 0)),
      JPEGCodec::FORMAT_BGRA, image_.width(),
      image_.height(),
      static_cast<int>(image_.rowBytes()), 90,
      &(jpeg_image_->data));
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

// TODO(meelapshah) fix the leak in these tests and re-enable.
#if 0

TEST_F(ThumbnailStoreTest, RetrieveFromCache) {
  RefCountedBytes* read_image = NULL;
  scoped_refptr<ThumbnailStore> store = new ThumbnailStore;

  store->file_path_ = file_path_;
  store->cache_.reset(new ThumbnailStore::Cache);
  store->cache_initialized_ = true;

  // Retrieve a thumbnail/score for a nonexistent page.

  EXPECT_FALSE(store->GetPageThumbnail(url2_, &read_image));

  // Store a thumbnail into the cache and retrieve it.

  EXPECT_TRUE(store->SetPageThumbnail(url1_, image_, score1_, false));
  EXPECT_TRUE(store->GetPageThumbnail(url1_, &read_image));
  EXPECT_TRUE(score1_.Equals((*store->cache_)[url1_].second));
  EXPECT_TRUE(read_image->data.size() == jpeg_image_->data.size());
  EXPECT_EQ(0, memcmp(&read_image->data[0], &jpeg_image_->data[0],
                      jpeg_image_->data.size()));

  read_image->Release();
}

TEST_F(ThumbnailStoreTest, RetrieveFromDisk) {
  scoped_refptr<RefCountedBytes> read_image = new RefCountedBytes;
  scoped_refptr<ThumbnailStore> store = new ThumbnailStore;

  store->file_path_ = file_path_;
  store->cache_.reset(new ThumbnailStore::Cache);
  store->cache_initialized_ = true;

  // Store a thumbnail onto the disk and retrieve it.

  EXPECT_TRUE(store->SetPageThumbnail(url1_, image_, score1_, false));
  EXPECT_TRUE(store->WriteThumbnailToDisk(url1_));
  EXPECT_TRUE(store->GetPageThumbnailFromDisk(
      file_path_.AppendASCII(url1_.host()), &url2_, read_image, &score2_));
  read_image->AddRef();
  EXPECT_TRUE(url1_ == url2_);
  EXPECT_TRUE(score1_.Equals(score2_));
  EXPECT_TRUE(read_image->data.size() == jpeg_image_->data.size());
  EXPECT_EQ(0, memcmp(&read_image->data[0], &jpeg_image_->data[0],
                      jpeg_image_->data.size()));
}
#endif
