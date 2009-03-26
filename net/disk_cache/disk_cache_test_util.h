// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DISK_CACHE_DISK_CACHE_TEST_UTIL_H_
#define NET_DISK_CACHE_DISK_CACHE_TEST_UTIL_H_

#include <string>

#include "base/file_path.h"
#include "base/message_loop.h"
#include "base/task.h"

// Re-creates a given test file inside the cache test folder.
bool CreateCacheTestFile(const wchar_t* name);

// Deletes all file son the cache.
bool DeleteCache(const wchar_t* path);

// Gets the path to the cache test folder.
std::wstring GetCachePath();

// Fills buffer with random values (may contain nulls unless no_nulls is true).
void CacheTestFillBuffer(char* buffer, size_t len, bool no_nulls);

// Deletes all files matching a pattern.
// Do not call this function with "*" as search_name.
bool DeleteFiles(const wchar_t* path, const wchar_t* search_name);

// Generates a random key of up to 200 bytes.
std::string GenerateKey(bool same_length);

// Returns true if the cache is not corrupt.
bool CheckCacheIntegrity(const std::wstring& path);

// Helper class which ensures that the cache dir returned by GetCachePath exists
// and is clear in ctor and that the directory gets deleted in dtor.
class ScopedTestCache {
 public:
  ScopedTestCache();
  ScopedTestCache(const std::wstring& name);  // Use a specific folder name.
  ~ScopedTestCache();

  FilePath path() const { return FilePath::FromWStringHack(path_); }
  std::wstring path_wstring() const { return path_; }

 private:
  const std::wstring path_;  // Path to the cache test folder.

  DISALLOW_COPY_AND_ASSIGN(ScopedTestCache);
};

// -----------------------------------------------------------------------

// Simple callback to process IO completions from the cache.
class CallbackTest : public CallbackRunner< Tuple1<int> >  {
 public:
  explicit CallbackTest(int id) : id_(id), reuse_(0) {}
  explicit CallbackTest(int id, bool reuse) : id_(id), reuse_(reuse ? 0 : 1) {}
  ~CallbackTest() {}

  virtual void RunWithParams(const Tuple1<int>& params);

 private:
  int id_;
  int reuse_;
  DISALLOW_COPY_AND_ASSIGN(CallbackTest);
};

// -----------------------------------------------------------------------

// Simple helper to deal with the message loop on a test.
class MessageLoopHelper {
 public:
  MessageLoopHelper();

  // Run the message loop and wait for num_callbacks before returning. Returns
  // false if we are waiting to long.
  bool WaitUntilCacheIoFinished(int num_callbacks);

 private:
  // Sets the number of callbacks that can be received so far.
  void ExpectCallbacks(int num_callbacks) {
    num_callbacks_ = num_callbacks;
    num_iterations_ = last_ = 0;
    completed_ = false;
  }

  // Called periodically to test if WaitUntilCacheIoFinished should return.
  void TimerExpired();

  base::RepeatingTimer<MessageLoopHelper> timer_;
  int num_callbacks_;
  int num_iterations_;
  int last_;
  bool completed_;

  DISALLOW_COPY_AND_ASSIGN(MessageLoopHelper);
};

#endif  // NET_DISK_CACHE_DISK_CACHE_TEST_UTIL_H_
