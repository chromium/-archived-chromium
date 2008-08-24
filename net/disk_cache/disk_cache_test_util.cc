// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/disk_cache/disk_cache_test_util.h"

#include "base/file_util.h"
#include "base/path_service.h"
#include "net/disk_cache/backend_impl.h"
#include "net/disk_cache/cache_util.h"
#include "net/disk_cache/file.h"

std::string GenerateKey(bool same_length) {
  char key[200];
  CacheTestFillBuffer(key, sizeof(key), same_length);

  key[199] = '\0';
  return std::string(key);
}

void CacheTestFillBuffer(char* buffer, size_t len, bool no_nulls) {
  static bool called = false;
  if (!called) {
    called = true;
    int seed = static_cast<int>(Time::Now().ToInternalValue());
    srand(seed);
  }

  for (size_t i = 0; i < len; i++) {
    buffer[i] = static_cast<char>(rand());
    if (!buffer[i] && no_nulls)
      buffer[i] = 'g';
  }
  if (len && !buffer[0])
    buffer[0] = 'g';
}

std::wstring GetCachePath() {
  std::wstring path;
  PathService::Get(base::DIR_TEMP, &path);
  file_util::AppendToPath(&path, L"cache_test");
  if (!file_util::PathExists(path))
    file_util::CreateDirectory(path);

  return path;
}

bool CreateCacheTestFile(const wchar_t* name) {
  using namespace disk_cache;
  int flags = OS_FILE_CREATE_ALWAYS | OS_FILE_READ | OS_FILE_WRITE |
              OS_FILE_SHARE_READ | OS_FILE_SHARE_WRITE;

  scoped_refptr<File> file(new File(CreateOSFile(name, flags, NULL)));
  if (!file->IsValid())
    return false;

  file->SetLength(4 * 1024 * 1024);
  return true;
}

bool DeleteCache(const wchar_t* path) {
  disk_cache::DeleteCache(path, false);
  return true;
}

bool CheckCacheIntegrity(const std::wstring& path) {
  scoped_ptr<disk_cache::BackendImpl> cache(new disk_cache::BackendImpl(path));
  if (!cache.get())
    return false;
  if (!cache->Init())
    return false;
  return cache->SelfCheck() >= 0;
}

// -----------------------------------------------------------------------

int g_cache_tests_max_id = 0;
volatile int g_cache_tests_received = 0;
volatile bool g_cache_tests_error = 0;

// On the actual callback, increase the number of tests received and check for
// errors (an unexpected test received)
void CallbackTest::RunWithParams(const Tuple1<int>& params) {
  if (id_ > g_cache_tests_max_id) {
    NOTREACHED();
    g_cache_tests_error = true;
  } else if (reuse_) {
    DCHECK(1 == reuse_);
    if (2 == reuse_)
      g_cache_tests_error = true;
    reuse_++;
  }

  g_cache_tests_received++;
}

// -----------------------------------------------------------------------

// Quits the message loop when all callbacks are called or we've been waiting
// too long for them (2 secs without a callback).
void TimerTask::Run() {
  if (g_cache_tests_received > num_callbacks_) {
    NOTREACHED();
  } else if (g_cache_tests_received == num_callbacks_) {
    completed_ = true;
    MessageLoop::current()->Quit();
  } else {
    // Not finished yet. See if we have to abort.
    if (last_ == g_cache_tests_received)
      num_iterations_++;
    else
      last_ = g_cache_tests_received;
    if (40 == num_iterations_)
      MessageLoop::current()->Quit();
  }
}

// -----------------------------------------------------------------------

MessageLoopHelper::MessageLoopHelper() {
  message_loop_ = MessageLoop::current();
  // Create a recurrent timer of 50 mS.
  timer_ = message_loop_->timer_manager()->StartTimer(50, &timer_task_, true);
}

MessageLoopHelper::~MessageLoopHelper() {
  message_loop_->timer_manager()->StopTimer(timer_);
  delete timer_;
}

bool MessageLoopHelper::WaitUntilCacheIoFinished(int num_callbacks) {
  if (num_callbacks == g_cache_tests_received)
    return true;

  timer_task_.ExpectCallbacks(num_callbacks);
  message_loop_->Run();
  return timer_task_.GetSate();
}


