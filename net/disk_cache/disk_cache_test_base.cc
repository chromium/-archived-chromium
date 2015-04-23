// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/disk_cache/disk_cache_test_base.h"

#include "net/disk_cache/backend_impl.h"
#include "net/disk_cache/disk_cache_test_util.h"
#include "net/disk_cache/mem_backend_impl.h"

void DiskCacheTest::TearDown() {
  MessageLoop::current()->RunAllPending();
}

void DiskCacheTestWithCache::SetMaxSize(int size) {
  size_ = size;
  if (cache_impl_)
    EXPECT_TRUE(cache_impl_->SetMaxSize(size));

  if (mem_cache_)
    EXPECT_TRUE(mem_cache_->SetMaxSize(size));
}

void DiskCacheTestWithCache::InitCache() {
  if (mask_ || new_eviction_)
    implementation_ = true;

  if (memory_only_)
    InitMemoryCache();
  else
    InitDiskCache();

  ASSERT_TRUE(NULL != cache_);
  if (first_cleanup_)
    ASSERT_EQ(0, cache_->GetEntryCount());
}

void DiskCacheTestWithCache::InitMemoryCache() {
  if (!implementation_) {
    cache_ = disk_cache::CreateInMemoryCacheBackend(size_);
    return;
  }

  mem_cache_ = new disk_cache::MemBackendImpl();
  cache_ = mem_cache_;
  ASSERT_TRUE(NULL != cache_);

  if (size_)
    EXPECT_TRUE(mem_cache_->SetMaxSize(size_));

  ASSERT_TRUE(mem_cache_->Init());
}

void DiskCacheTestWithCache::InitDiskCache() {
  std::wstring path = GetCachePath();
  if (first_cleanup_)
    ASSERT_TRUE(DeleteCache(path.c_str()));

  if (!implementation_) {
    cache_ = disk_cache::CreateCacheBackend(path, force_creation_, size_,
                                            net::DISK_CACHE);
    return;
  }

  InitDiskCacheImpl(path);
}

void DiskCacheTestWithCache::InitDiskCacheImpl(const std::wstring& path) {
  if (mask_)
    cache_impl_ = new disk_cache::BackendImpl(path, mask_);
  else
    cache_impl_ = new disk_cache::BackendImpl(path);

  cache_ = cache_impl_;
  ASSERT_TRUE(NULL != cache_);

  if (size_)
    EXPECT_TRUE(cache_impl_->SetMaxSize(size_));

  if (new_eviction_)
    cache_impl_->SetNewEviction();

  ASSERT_TRUE(cache_impl_->Init());
}

void DiskCacheTestWithCache::TearDown() {
  MessageLoop::current()->RunAllPending();
  delete cache_;

  if (!memory_only_ && integrity_) {
    std::wstring path = GetCachePath();
    EXPECT_TRUE(CheckCacheIntegrity(path, new_eviction_));
  }

  PlatformTest::TearDown();
}

// We are expected to leak memory when simulating crashes.
void DiskCacheTestWithCache::SimulateCrash() {
  ASSERT_TRUE(implementation_ && !memory_only_);
  cache_impl_->ClearRefCountForTest();

  delete cache_impl_;
  std::wstring path = GetCachePath();
  EXPECT_TRUE(CheckCacheIntegrity(path, new_eviction_));

  InitDiskCacheImpl(path);
}

void DiskCacheTestWithCache::SetTestMode() {
  ASSERT_TRUE(implementation_ && !memory_only_);
  cache_impl_->SetUnitTestMode();
}
