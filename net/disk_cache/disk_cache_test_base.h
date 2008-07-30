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

#ifndef NET_DISK_CACHE_DISK_CACHE_TEST_BASE_H__
#define NET_DISK_CACHE_DISK_CACHE_TEST_BASE_H__

#include "base/basictypes.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace disk_cache {

class Backend;
class BackendImpl;
class MemBackendImpl;

}

// Provides basic support for cache related tests.
class DiskCacheTestBase : public testing::Test {
 protected:
  DiskCacheTestBase()
      : cache_(NULL), cache_impl_(NULL), mem_cache_(NULL), mask_(0), size_(0),
        memory_only_(false), implementation_(false), force_creation_(false),
        first_cleanup_(true) {}

  void InitCache();
  virtual void TearDown();
  void SimulateCrash();
  void SetTestMode();

  void SetMemoryOnlyMode() {
    memory_only_ = true;
  }

  // Use the implementation directly instead of the factory provided object.
  void SetDirectMode() {
    implementation_ = true;
  }

  void SetMask(uint32 mask) {
    mask_ = mask;
  }

  void SetMaxSize(int size);

  // Deletes and re-creates the files on initialization errors.
  void SetForceCreation() {
    force_creation_ = true;
  }

  void DisableFirstCleanup() {
    first_cleanup_ = false;
  }

  // cache_ will always have a valid object, regardless of how the cache was
  // initialized. The implementation pointers can be NULL.
  disk_cache::Backend* cache_;
  disk_cache::BackendImpl* cache_impl_;
  disk_cache::MemBackendImpl* mem_cache_;

  uint32 mask_;
  int size_;
  bool memory_only_;
  bool implementation_;
  bool force_creation_;
  bool first_cleanup_;

 private:
  void InitMemoryCache();
  void InitDiskCache();
};

#endif  // NET_DISK_CACHE_DISK_CACHE_TEST_BASE_H__
