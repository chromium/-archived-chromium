// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/thumbnail_store.h"

#include <string.h>

#include "base/basictypes.h"
#include "base/pickle.h"
#include "base/file_util.h"
#include "base/gfx/jpeg_codec.h"
#include "base/thread.h"
#include "chrome/browser/browser_process.h"
#include "chrome/common/thumbnail_score.h"
#include "googleurl/src/gurl.h"
#include "third_party/skia/include/core/SkBitmap.h"

ThumbnailStore::ThumbnailStore() : cache_(NULL), cache_initialized_(false) {
}

ThumbnailStore::~ThumbnailStore() {
}

void ThumbnailStore::Init(const FilePath& file_path) {
  file_path_ = file_path.DirName();
  g_browser_process->file_thread()->message_loop()->PostTask(FROM_HERE,
      NewRunnableMethod(this, &ThumbnailStore::GetAllThumbnailsFromDisk,
                        file_path_, MessageLoop::current()));
}

void ThumbnailStore::OnDiskDataAvailable(ThumbnailStore::Cache* cache) {
  if (cache) {
    cache_.reset(cache);
    cache_initialized_ = true;
  }
}

void ThumbnailStore::GetAllThumbnailsFromDisk(FilePath filepath,
                                              MessageLoop* cb_loop) {
  // Create the specified directory if it does not exist.
  if (!file_util::DirectoryExists(filepath) &&
      !file_util::CreateDirectory(filepath))
    return;

  // Walk the directory and read the thumbnail data from disk.
  FilePath path;
  GURL url;
  SkBitmap image;
  ThumbnailScore score;
  ThumbnailStore::Cache* cache = new ThumbnailStore::Cache;
  file_util::FileEnumerator fenum(filepath, false,
                                  file_util::FileEnumerator::FILES);

  while (!(path = fenum.Next()).empty()) {
    if (GetPageThumbnailFromDisk(path, &url, &image, &score))
      (*cache)[url] = std::make_pair(image, score);
  }

  cb_loop->PostTask(FROM_HERE,
      NewRunnableMethod(this, &ThumbnailStore::OnDiskDataAvailable, cache));
}

bool ThumbnailStore::GetPageThumbnailFromDisk(const FilePath& file,
                                              GURL* url, SkBitmap* thumbnail,
                                              ThumbnailScore* score) const {
  int64 file_size;
  if (!file_util::GetFileSize(file, &file_size))
    return false;

  // Read the file into a buffer.
  std::vector<char> data;
  data.resize(static_cast<unsigned int>(file_size));
  if (file_util::ReadFile(file, &data[0], static_cast<int>(file_size)) == -1)
    return false;

  // Unpack the url, ThumbnailScore and JPEG size from the buffer.
  std::string url_string;
  unsigned int jpeg_len;
  void* iter = NULL;
  Pickle packed(&data[0], static_cast<int>(file_size));

  if (!packed.ReadString(&iter, &url_string) ||
      !UnpackScore(score, packed, iter) ||
      !packed.ReadUInt32(&iter, &jpeg_len))
    return false;

  // Store the url to the out parameter.
  GURL temp_url(url_string);
  url->Swap(&temp_url);

  // Unpack the JPEG data from the buffer.
  if (thumbnail) {
    const char* jpeg_data = NULL;
    int out_len;

    if (!packed.ReadData(&iter, &jpeg_data, &out_len) ||
        out_len != jpeg_len)
      return false;

    // Convert the jpeg_data to an SkBitmap.
    SkBitmap* thumbnail_ = JPEGCodec::Decode(
        reinterpret_cast<const unsigned char*>(jpeg_data), jpeg_len);
    *thumbnail = *thumbnail_;
    delete thumbnail_;
  }
  return true;
}

bool ThumbnailStore::SetPageThumbnail(const GURL& url,
                                      SkBitmap& thumbnail,
                                      const ThumbnailScore& score,
                                      bool write_to_disk) {
  if (!cache_initialized_)
    return false;

  // If a thumbnail already exists, check if it should be replaced.
  if (cache_->find(url) != cache_->end() &&
      !ShouldReplaceThumbnailWith((*cache_)[url].second, score))
    return true;

  // Update the cache_ with the new thumbnail.
  (*cache_)[url] = std::make_pair(thumbnail, score);

  // Write the new thumbnail data to disk in the background on file_thread.
  if (write_to_disk) {
    g_browser_process->file_thread()->message_loop()->PostTask(FROM_HERE,
        NewRunnableMethod(this, &ThumbnailStore::WriteThumbnailToDisk, url));
  }
  return true;
}

bool ThumbnailStore::WriteThumbnailToDisk(const GURL& url) const {
  // Thumbnail data will be stored in a file named url.host().
  FilePath file = file_path_.AppendASCII(url.host());
  Pickle packed;
  SkBitmap thumbnail = (*cache_)[url].first;
  ThumbnailScore score = (*cache_)[url].second;

  // Convert the SkBitmap to a JPEG.
  std::vector<unsigned char> jpeg_data;
  SkAutoLockPixels thumbnail_lock(thumbnail);
  bool encoded = JPEGCodec::Encode(
      reinterpret_cast<unsigned char*>(thumbnail.getAddr32(0, 0)),
      JPEGCodec::FORMAT_BGRA, thumbnail.width(),
      thumbnail.height(),
      static_cast<int>(thumbnail.rowBytes()), 90,
      &jpeg_data);

  if (!encoded)
    return false;

  // Pack the url, ThumbnailScore, and the JPEG data.
  packed.WriteString(url.spec());
  PackScore(score, &packed);
  packed.WriteUInt32(jpeg_data.size());
  packed.WriteData(reinterpret_cast<char*>(&jpeg_data[0]), jpeg_data.size());

  // Write the packed data to a file.
  file_util::Delete(file, false);
  return file_util::WriteFile(file,
                              reinterpret_cast<const char*>(packed.data()),
                              packed.size()) != -1;
}

bool ThumbnailStore::GetPageThumbnail(const GURL& url, SkBitmap* thumbnail,
                                      ThumbnailScore* score) {
  if (!cache_initialized_ ||
      cache_->find(url) == cache_->end())
    return false;

  *thumbnail = (*cache_)[url].first;
  *score = (*cache_)[url].second;
  return true;
}

void ThumbnailStore::PackScore(const ThumbnailScore& score,
                               Pickle* packed) const {
  // Pack the contents of the given ThumbnailScore into the given Pickle.
  packed->WriteData(reinterpret_cast<const char*>(&score.boring_score),
                   sizeof(score.boring_score));
  packed->WriteBool(score.at_top);
  packed->WriteBool(score.good_clipping);
  packed->WriteInt64(score.time_at_snapshot.ToInternalValue());
}

bool ThumbnailStore::UnpackScore(ThumbnailScore* score, const Pickle& packed,
                                 void*& iter) const {
  // Unpack a ThumbnailScore from the given Pickle and iterator.
  const char* boring = NULL;
  int out_len;
  int64 us;

  if (!packed.ReadData(&iter, &boring, &out_len) ||
      !packed.ReadBool(&iter, &score->at_top) ||
      !packed.ReadBool(&iter, &score->good_clipping) ||
      !packed.ReadInt64(&iter, &us))
    return false;

  if (out_len != sizeof(score->boring_score))
    return false;

  memcpy(&score->boring_score, boring, sizeof(score->boring_score));
  score->time_at_snapshot = base::Time::FromInternalValue(us);
  return true;
}
