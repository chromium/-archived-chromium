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
#include "chrome/common/chrome_paths.h"
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

  SkBitmap read_image_, image_enc_dec_, image_;
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

  // The ThumbnailStore will encode the thumbnail to jpeg at 90% quality, so
  // we do this to image.  When image is stored, image_enc_dec_ is what
  // ThumbnailStore should return.
  std::vector<unsigned char> jpeg_data;
  SkAutoLockPixels thumbnail_lock(image_);
  bool encoded = JPEGCodec::Encode(
      reinterpret_cast<unsigned char*>(image_.getAddr32(0, 0)),
      JPEGCodec::FORMAT_BGRA, image_.width(),
      image_.height(),
      static_cast<int>(image_.rowBytes()), 90,
      &jpeg_data);

  image_enc_dec_ = *(JPEGCodec::Decode(
                        reinterpret_cast<const unsigned char*>(&jpeg_data[0]),
                        jpeg_data.size()));
}

void ThumbnailStoreTest::PrintPixelDiff(SkBitmap* image_a, SkBitmap* image_b) {
  // Compute the maximum difference in each of the RGBA components across all
  // pixels between the retrieved SkBitmap and the original.  These
  // differences should be small since encoding was done at 90% before
  // writing to disk.

  SkAutoLockPixels lock_a(*image_a);
  SkAutoLockPixels lock_b(*image_b);

  int ppr = read_image_.rowBytesAsPixels();
  unsigned int *a, *b;
  unsigned int maxv[4];
  memset(maxv, 0, sizeof(maxv));

  for (int nrows = read_image_.height()-1; nrows >= 0; nrows--) {
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

TEST_F(ThumbnailStoreTest, RetrieveFromCache) {
  ThumbnailStore* store = new ThumbnailStore;
  store->AddRef();
  store->file_path_ = file_path_;
  store->cache_.reset(new ThumbnailStore::Cache);
  store->cache_initialized_ = true;

  read_image_.reset();

  // Retrieve a thumbnail/score for a nonexistent page.

  EXPECT_FALSE(store->GetPageThumbnail(url2_, &read_image_, &score2_));

  // Store a thumbnail into the cache and retrieve it.

  EXPECT_TRUE(store->SetPageThumbnail(url1_, image_, score1_, false));
  EXPECT_TRUE(store->GetPageThumbnail(url1_, &read_image_, &score2_));
  EXPECT_TRUE(score1_.Equals(score2_));
  EXPECT_TRUE(read_image_.getSize() == image_.getSize());
  EXPECT_TRUE(read_image_.pixelRef() == image_.pixelRef());
  EXPECT_FALSE(read_image_.isNull());

  store->Release();
}

TEST_F(ThumbnailStoreTest, RetrieveFromDisk) {
  ThumbnailStore* store = new ThumbnailStore;
  store->AddRef();
  store->file_path_ = file_path_;
  store->cache_.reset(new ThumbnailStore::Cache);
  store->cache_initialized_ = true;

  read_image_.reset();

  // Store a thumbnail onto the disk and retrieve it.

  EXPECT_TRUE(store->SetPageThumbnail(url1_, image_, score1_, false));
  EXPECT_TRUE(store->WriteThumbnailToDisk(url1_));
  EXPECT_TRUE(store->GetPageThumbnailFromDisk(
      file_path_.AppendASCII(url1_.host()), &url2_, &read_image_, &score2_));
  EXPECT_TRUE(url1_ == url2_);
  EXPECT_TRUE(score1_.Equals(score2_));
  EXPECT_TRUE(read_image_.getSize() == image_enc_dec_.getSize());
  EXPECT_FALSE(read_image_.isNull());

  // The retrieved SkBitmap should be the same as the original image
  // encoded at 90% quality to jpeg, then decoded.

  {
    SkAutoLockPixels lock_a(read_image_);
    SkAutoLockPixels lock_b(image_enc_dec_);
    EXPECT_TRUE(0 == memcmp(read_image_.getPixels(),
                            image_enc_dec_.getPixels(),
                            read_image_.getSize()));
  }
  PrintPixelDiff(&read_image_, &image_);

  store->Release();
}
