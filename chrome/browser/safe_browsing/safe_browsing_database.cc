// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/safe_browsing_database.h"

#include "base/command_line.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/sha2.h"
#include "chrome/browser/safe_browsing/safe_browsing_database_impl.h"
#include "chrome/browser/safe_browsing/safe_browsing_database_bloom.h"
#include "chrome/common/chrome_switches.h"
#include "googleurl/src/gurl.h"

using base::Time;

// Filename suffix for the bloom filter.
static const FilePath::CharType kBloomFilterFile[] =
    FILE_PATH_LITERAL(" Filter");

// Factory method.
SafeBrowsingDatabase* SafeBrowsingDatabase::Create() {
  if (CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kUseOldSafeBrowsing)) {
    return new SafeBrowsingDatabaseImpl;
  }
  return new SafeBrowsingDatabaseBloom;
}

bool SafeBrowsingDatabase::NeedToCheckUrl(const GURL& url) {
  // Keep a reference to the current bloom filter in case the database rebuilds
  // it while we're accessing it.
  scoped_refptr<BloomFilter> filter = bloom_filter_;
  if (!filter.get())
    return true;

  IncrementBloomFilterReadCount();

  std::vector<std::string> hosts;
  safe_browsing_util::GenerateHostsToCheck(url, &hosts);
  if (hosts.size() == 0)
    return false;  // Could be about:blank.

  SBPrefix host_key;
  if (url.HostIsIPAddress()) {
    base::SHA256HashString(url.host() + "/", &host_key, sizeof(SBPrefix));
    if (filter->Exists(host_key))
      return true;
  } else {
    base::SHA256HashString(hosts[0] + "/", &host_key, sizeof(SBPrefix));
    if (filter->Exists(host_key))
      return true;

    if (hosts.size() > 1) {
      base::SHA256HashString(hosts[1] + "/", &host_key, sizeof(SBPrefix));
      if (filter->Exists(host_key))
        return true;
    }
  }
  return false;
}

// static
FilePath SafeBrowsingDatabase::BloomFilterFilename(
    const FilePath& db_filename) {
  return FilePath(db_filename.value() + kBloomFilterFile);
}

void SafeBrowsingDatabase::LoadBloomFilter() {
  DCHECK(!bloom_filter_filename_.empty());

  int64 size_64;
  if (!file_util::GetFileSize(bloom_filter_filename_, &size_64) ||
      size_64 == 0) {
    BuildBloomFilter();
    return;
  }

  int size = static_cast<int>(size_64);
  char* data = new char[size];
  CHECK(data);

  Time before = Time::Now();
  file_util::ReadFile(bloom_filter_filename_, data, size);
  SB_DLOG(INFO) << "SafeBrowsingDatabase read bloom filter in " <<
        (Time::Now() - before).InMilliseconds() << " ms";

  bloom_filter_ = new BloomFilter(data, size);
}

void SafeBrowsingDatabase::DeleteBloomFilter() {
  file_util::Delete(bloom_filter_filename_, false);
}

void SafeBrowsingDatabase::WriteBloomFilter() {
  if (!bloom_filter_.get())
    return;

  Time before = Time::Now();
  file_util::WriteFile(bloom_filter_filename_,
                       bloom_filter_->data(),
                       bloom_filter_->size());
  SB_DLOG(INFO) << "SafeBrowsingDatabase wrote bloom filter in " <<
      (Time::Now() - before).InMilliseconds() << " ms";
}
