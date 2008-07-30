// Copyright 2008, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "net/disk_cache/disk_cache_test_base.h"

#include "net/disk_cache/backend_impl.h"
#include "net/disk_cache/disk_cache_test_util.h"
#include "net/disk_cache/mem_backend_impl.h"

void DiskCacheTestBase::SetMaxSize(int size) {
  size_ = size;
  if (cache_impl_)
    EXPECT_TRUE(cache_impl_->SetMaxSize(size));

  if (mem_cache_)
    EXPECT_TRUE(mem_cache_->SetMaxSize(size));
}

void DiskCacheTestBase::InitCache() {
  if (mask_)
    implementation_ = true;

  if (memory_only_)
    InitMemoryCache();
  else
    InitDiskCache();

  ASSERT_TRUE(NULL != cache_);
  if (first_cleanup_)
    ASSERT_EQ(0, cache_->GetEntryCount());
}

void DiskCacheTestBase::InitMemoryCache() {
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

void DiskCacheTestBase::InitDiskCache() {
  std::wstring path = GetCachePath();
  if (first_cleanup_)
    ASSERT_TRUE(DeleteCache(path.c_str()));

  if (!implementation_) {
    cache_ = disk_cache::CreateCacheBackend(path, force_creation_, size_);
    return;
  }

  if (mask_)
    cache_impl_ = new disk_cache::BackendImpl(path, mask_);
  else
    cache_impl_ = new disk_cache::BackendImpl(path);

  cache_ = cache_impl_;
  ASSERT_TRUE(NULL != cache_);

  if (size_)
    EXPECT_TRUE(cache_impl_->SetMaxSize(size_));

  ASSERT_TRUE(cache_impl_->Init());
}


void DiskCacheTestBase::TearDown() {
  delete cache_;

  if (!memory_only_) {
    std::wstring path = GetCachePath();
    EXPECT_TRUE(CheckCacheIntegrity(path));
  }
}

// We are expected to leak memory when simulating crashes.
void DiskCacheTestBase::SimulateCrash() {
  ASSERT_TRUE(implementation_ && !memory_only_);
  cache_impl_->ClearRefCountForTest();

  delete cache_impl_;
  std::wstring path = GetCachePath();
  EXPECT_TRUE(CheckCacheIntegrity(path));

  if (mask_)
    cache_impl_ = new disk_cache::BackendImpl(path, mask_);
  else
    cache_impl_ = new disk_cache::BackendImpl(path);
  cache_ = cache_impl_;
  ASSERT_TRUE(NULL != cache_);

  if (size_)
    cache_impl_->SetMaxSize(size_);
  ASSERT_TRUE(cache_impl_->Init());
}

void DiskCacheTestBase::SetTestMode() {
  ASSERT_TRUE(implementation_ && !memory_only_);
  cache_impl_->SetUnitTestMode();
}
