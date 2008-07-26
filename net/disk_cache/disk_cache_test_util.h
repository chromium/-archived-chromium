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

#ifndef NET_DISK_CACHE_DISK_CACHE_TEST_UTIL_H__
#define NET_DISK_CACHE_DISK_CACHE_TEST_UTIL_H__

#include <string>

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
  DISALLOW_EVIL_CONSTRUCTORS(CallbackTest);
};

// -----------------------------------------------------------------------

// We'll use a timer to fire from time to time to check the number of IO
// operations finished so far.
class TimerTask : public Task {
 public:
  TimerTask() : num_callbacks_(0), num_iterations_(0) {}
  ~TimerTask() {}

  virtual void Run();

  // Sets the number of callbacks that can be received so far.
  void ExpectCallbacks(int num_callbacks) {
    num_callbacks_ = num_callbacks;
    num_iterations_ = last_ = 0;
    completed_ = false;
  }

  // Returns true if all callbacks were invoked.
  bool GetSate() {
    return completed_;
  }

 private:
  int num_callbacks_;
  int num_iterations_;
  int last_;
  bool completed_;
  DISALLOW_EVIL_CONSTRUCTORS(TimerTask);
};

// -----------------------------------------------------------------------

// Simple helper to deal with the message loop on a test.
class MessageLoopHelper {
 public:
  MessageLoopHelper();
  ~MessageLoopHelper();

  // Run the message loop and wait for num_callbacks before returning. Returns
  // false if we are waiting to long.
  bool WaitUntilCacheIoFinished(int num_callbacks);

 private:
  MessageLoop* message_loop_;
  Timer* timer_;
  TimerTask timer_task_;
  DISALLOW_EVIL_CONSTRUCTORS(MessageLoopHelper);
};

#endif  // NET_DISK_CACHE_DISK_CACHE_TEST_UTIL_H__
