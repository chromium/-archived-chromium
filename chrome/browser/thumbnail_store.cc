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
  file_path_ = file_path;
  g_browser_process->file_thread()->message_loop()->PostTask(FROM_HERE,
      NewRunnableMethod(this, &ThumbnailStore::GetAllThumbnailsFromDisk,
                        file_path_, MessageLoop::current()));
}

void ThumbnailStore::GetAllThumbnailsFromDisk(FilePath filepath,
                                              MessageLoop* cb_loop) {
  ThumbnailStore::Cache* cache = new ThumbnailStore::Cache;

  // Create the specified directory if it does not exist.
  if (!file_util::DirectoryExists(filepath)) {
    file_util::CreateDirectory(filepath);
  } else {
    // Walk the directory and read the thumbnail data from disk.
    FilePath path;
    GURL url;
    RefCountedBytes* data = new RefCountedBytes;
    ThumbnailScore score;
    file_util::FileEnumerator fenum(filepath, false,
                                    file_util::FileEnumerator::FILES);

    while (!(path = fenum.Next()).empty()) {
      if (GetPageThumbnailFromDisk(path, &url, data, &score))
        (*cache)[url] = std::make_pair(data, score);
    }
  }
  cb_loop->PostTask(FROM_HERE,
      NewRunnableMethod(this, &ThumbnailStore::OnDiskDataAvailable, cache));
}

bool ThumbnailStore::GetPageThumbnailFromDisk(const FilePath& file,
                                              GURL* url,
                                              RefCountedBytes* data,
                                              ThumbnailScore* score) const {
  int64 file_size;
  if (!file_util::GetFileSize(file, &file_size))
    return false;

  // Read the file into a buffer.
  std::vector<char> file_data;
  file_data.resize(static_cast<unsigned int>(file_size));
  if (file_util::ReadFile(file, &file_data[0],
                          static_cast<int>(file_size)) == -1)
    return false;

  // Unpack the url, ThumbnailScore and JPEG size from the buffer.
  std::string url_string;
  unsigned int jpeg_len;
  void* iter = NULL;
  Pickle packed(&file_data[0], static_cast<int>(file_size));

  if (!packed.ReadString(&iter, &url_string) ||
      !UnpackScore(score, packed, iter) ||
      !packed.ReadUInt32(&iter, &jpeg_len))
    return false;

  // Store the url to the out parameter.
  GURL temp_url(url_string);
  url->Swap(&temp_url);

  // Unpack the JPEG data from the buffer.
  const char* jpeg_data = NULL;
  int out_len;

  if (!packed.ReadData(&iter, &jpeg_data, &out_len) ||
      out_len != jpeg_len)
    return false;

  // Copy jpeg data to the out parameter.
  data->data.resize(jpeg_len);
  memcpy(&data->data[0], jpeg_data, jpeg_len);
  return true;
}

void ThumbnailStore::OnDiskDataAvailable(ThumbnailStore::Cache* cache) {
  if (cache) {
    cache_.reset(cache);
    cache_initialized_ = true;
  }
}

bool ThumbnailStore::SetPageThumbnail(const GURL& url,
                                      const SkBitmap& thumbnail,
                                      const ThumbnailScore& score,
                                      bool write_to_disk) {
  if (!cache_initialized_)
    return false;

  // If a thumbnail already exists, check if it should be replaced.
  if (cache_->find(url) != cache_->end() &&
      !ShouldReplaceThumbnailWith((*cache_)[url].second, score))
    return true;

  base::TimeTicks encode_start = base::TimeTicks::Now();

  // Encode the SkBitmap to jpeg and add to cache.
  RefCountedBytes* jpeg_data = new RefCountedBytes;
  SkAutoLockPixels thumbnail_lock(thumbnail);
  bool encoded = JPEGCodec::Encode(
      reinterpret_cast<unsigned char*>(thumbnail.getAddr32(0, 0)),
      JPEGCodec::FORMAT_BGRA, thumbnail.width(),
      thumbnail.height(),
      static_cast<int>(thumbnail.rowBytes()), 90,
      &jpeg_data->data);

  base::TimeDelta delta = base::TimeTicks::Now() - encode_start;
  HISTOGRAM_TIMES("Thumbnail.Encode", delta);

  if (!encoded)
    return false;

  // Update the cache_ with the new thumbnail.
  (*cache_)[url] = std::make_pair(jpeg_data, score);

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
  scoped_refptr<RefCountedBytes> data((*cache_)[url].first);
  ThumbnailScore score = (*cache_)[url].second;

  // Pack the url, ThumbnailScore, and the JPEG data.
  packed.WriteString(url.spec());
  PackScore(score, &packed);
  packed.WriteUInt32(data->data.size());
  packed.WriteData(reinterpret_cast<char*>(&data->data[0]), data->data.size());

  // Write the packed data to a file.
  file_util::Delete(file, false);
  return file_util::WriteFile(file,
                              reinterpret_cast<const char*>(packed.data()),
                              packed.size()) != -1;
}

bool ThumbnailStore::GetPageThumbnail(const GURL& url, RefCountedBytes** data) {
  if (!cache_initialized_ ||
      cache_->find(url) == cache_->end())
    return false;

  *data = (*cache_)[url].first;
  (*data)->AddRef();
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
