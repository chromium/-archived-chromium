// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/thumbnail_store.h"

#include <string.h>
#include <algorithm>

#include "base/basictypes.h"
#include "base/pickle.h"
#include "base/file_util.h"
#include "base/gfx/jpeg_codec.h"
#include "base/md5.h"
#include "base/string_util.h"
#include "base/thread.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profile.h"
#include "chrome/common/pref_service.h"
#include "chrome/common/thumbnail_score.h"
#include "googleurl/src/gurl.h"
#include "third_party/skia/include/core/SkBitmap.h"


ThumbnailStore::ThumbnailStore()
    : cache_(NULL),
      cache_initialized_(false),
      hs_(NULL),
      url_blacklist_(NULL) {
}

ThumbnailStore::~ThumbnailStore() {
}

void ThumbnailStore::Init(const FilePath& file_path, Profile* profile) {
  file_path_ = file_path;
  hs_ = profile->GetHistoryService(Profile::EXPLICIT_ACCESS);
  url_blacklist_ = profile->GetPrefs()->
      GetMutableDictionary(prefs::kNTPMostVisitedURLsBlacklist);

  g_browser_process->file_thread()->message_loop()->PostTask(FROM_HERE,
      NewRunnableMethod(this, &ThumbnailStore::GetAllThumbnailsFromDisk,
                        file_path_, MessageLoop::current()));

  timer_.Start(base::TimeDelta::FromMinutes(30), this,
               &ThumbnailStore::UpdateURLData);
  UpdateURLData();
}

bool ThumbnailStore::SetPageThumbnail(const GURL& url,
                                      const SkBitmap& thumbnail,
                                      const ThumbnailScore& score,
                                      bool write_to_disk) {
  if (!cache_initialized_)
    return false;

  if (!ShouldStoreThumbnailForURL(url) ||
      (cache_->find(url) != cache_->end() &&
      !ShouldReplaceThumbnailWith((*cache_)[url].second, score)))
    return true;

  base::TimeTicks encode_start = base::TimeTicks::Now();

  // Encode the SkBitmap to jpeg and add to cache.
  scoped_refptr<RefCountedBytes> jpeg_data = new RefCountedBytes;
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
        NewRunnableMethod(this, &ThumbnailStore::WriteThumbnailToDisk, url,
        jpeg_data, score));
  }
  return true;
}

bool ThumbnailStore::GetPageThumbnail(
    const GURL& url,
    RefCountedBytes** data) {
  if (!cache_initialized_ || IsURLBlacklisted(url))
    return false;

  // Look up the |url| in the redirect list to find the final destination
  // which is the key into the |cache_|.
  history::RedirectMap::iterator it = redirect_urls_->find(url);
  if (it == redirect_urls_->end())
    return false;

  // Return the first available thumbnail starting at the end of the
  // redirect list.
  history::RedirectList::reverse_iterator rit;
  for (rit = it->second->data.rbegin();
       rit != it->second->data.rend(); ++rit) {
    if (cache_->find(*rit) != cache_->end()) {
      *data = (*cache_)[*rit].first;
      (*data)->AddRef();
      return true;
    }
  }

  // TODO(meelapshah) bug 14643: check past redirect lists

  if (cache_->find(url) == cache_->end())
    return false;

  *data = (*cache_)[url].first;
  (*data)->AddRef();
  return true;
}

void ThumbnailStore::UpdateURLData() {
  int result_count = ThumbnailStore::kMaxCacheSize + url_blacklist_->GetSize();
  hs_->QueryTopURLsAndRedirects(result_count, &consumer_,
      NewCallback(this, &ThumbnailStore::OnURLDataAvailable));
}

void ThumbnailStore::OnURLDataAvailable(std::vector<GURL>* urls,
                                        history::RedirectMap* redirects) {
  DCHECK(urls);
  DCHECK(redirects);

  most_visited_urls_.reset(new std::vector<GURL>(*urls));
  redirect_urls_.reset(new history::RedirectMap(*redirects));
  CleanCacheData();
}

void ThumbnailStore::CleanCacheData() {
  if (!cache_initialized_)
    return;

  // For each URL in the cache, search the RedirectMap for the originating URL.
  // If this URL is blacklisted or not in the most visited list, delete the
  // thumbnail data for it from the cache and from disk in the background.
  scoped_refptr<RefCountedVector<GURL> > old_urls = new RefCountedVector<GURL>;
  for (ThumbnailStore::Cache::iterator cache_it = cache_->begin();
       cache_it != cache_->end();) {
    const GURL* url = NULL;
    for (history::RedirectMap::iterator it = redirect_urls_->begin();
         it != redirect_urls_->end(); ++it) {
      if (cache_it->first == it->first ||
          (it->second->data.size() &&
          cache_it->first == it->second->data.back())) {
        url = &it->first;
        break;
      }
    }

    if (url == NULL || IsURLBlacklisted(*url) || !IsPopular(*url)) {
      old_urls->data.push_back(cache_it->first);
      cache_->erase(cache_it++);
    } else {
      cache_it++;
    }
  }

  if (old_urls->data.size()) {
    g_browser_process->file_thread()->message_loop()->PostTask(FROM_HERE,
        NewRunnableMethod(this, &ThumbnailStore::DeleteThumbnails, old_urls));
  }
}

void ThumbnailStore::DeleteThumbnails(
    scoped_refptr<RefCountedVector<GURL> > thumbnail_urls) const {
  for (std::vector<GURL>::iterator it = thumbnail_urls->data.begin();
       it != thumbnail_urls->data.end(); ++it)
    file_util::Delete(file_path_.AppendASCII(MD5String(it->spec())), false);
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
    RefCountedBytes* data;
    ThumbnailScore score;
    file_util::FileEnumerator fenum(filepath, false,
                                    file_util::FileEnumerator::FILES);

    while (!(path = fenum.Next()).empty()) {
      data = new RefCountedBytes;
      if (GetPageThumbnailFromDisk(path, &url, data, &score))
        (*cache)[url] = std::make_pair(data, score);
      else
        delete data;
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
      out_len != static_cast<int>(jpeg_len))
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

bool ThumbnailStore::WriteThumbnailToDisk(const GURL& url,
                                          scoped_refptr<RefCountedBytes> data,
                                          const ThumbnailScore& score) const {
  Pickle packed;
  FilePath file = file_path_.AppendASCII(MD5String(url.spec()));

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

bool ThumbnailStore::ShouldStoreThumbnailForURL(const GURL& url) const {
  if (IsURLBlacklisted(url) || cache_->size() >= ThumbnailStore::kMaxCacheSize)
    return false;

  return most_visited_urls_->size() < ThumbnailStore::kMaxCacheSize ||
         IsPopular(url);
}

bool ThumbnailStore::IsURLBlacklisted(const GURL& url) const {
  if (url_blacklist_)
    return url_blacklist_->HasKey(GetDictionaryKeyForURL(url.spec()));
  return false;
}

std::wstring ThumbnailStore::GetDictionaryKeyForURL(
    const std::string& url) const {
  return ASCIIToWide(MD5String(url));
}

bool ThumbnailStore::IsPopular(const GURL& url) const {
  return most_visited_urls_->end() != find(most_visited_urls_->begin(),
                                          most_visited_urls_->end(),
                                          url);
}
