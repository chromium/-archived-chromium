// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is a simple application that stress-tests the crash recovery of the disk
// cache. The main application starts a copy of itself on a loop, checking the
// exit code of the child process. When the child dies in an unexpected way,
// the main application quits.

// The child application has two threads: one to exercise the cache in an
// infinite loop, and another one to asynchronously kill the process.

#include <string>
#include <vector>

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/debug_util.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/path_service.h"
#include "base/platform_thread.h"
#include "base/process_util.h"
#include "base/string_util.h"
#include "base/thread.h"
#include "net/disk_cache/disk_cache.h"
#include "net/disk_cache/disk_cache_test_util.h"

using base::Time;

const int kError = -1;
const int kExpectedCrash = 100;

// Starts a new process.
int RunSlave(int iteration) {
  std::wstring exe;
  PathService::Get(base::FILE_EXE, &exe);

  CommandLine cmdline(exe);
  cmdline.AppendLooseValue(ASCIIToWide(IntToString(iteration)));

  base::ProcessHandle handle;
  if (!base::LaunchApp(cmdline, false, false, &handle)) {
    printf("Unable to run test\n");
    return kError;
  }

  int exit_code;
  if (!base::WaitForExitCode(handle, &exit_code)) {
    printf("Unable to get return code\n");
    return kError;
  }
  return exit_code;
}

// Main loop for the master process.
int MasterCode() {
  for (int i = 0; i < 100000; i++) {
    int ret = RunSlave(i);
    if (kExpectedCrash != ret)
      return ret;
  }

  printf("More than enough...\n");

  return 0;
}

// -----------------------------------------------------------------------

// This thread will loop forever, adding and removing entries from the cache.
// iteration is the current crash cycle, so the entries on the cache are marked
// to know which instance of the application wrote them.
void StressTheCache(int iteration) {
  int cache_size = 0x800000;  // 8MB
  std::wstring path = GetCachePath();
  path.append(L"_stress");
  disk_cache::Backend* cache = disk_cache::CreateCacheBackend(path, false,
                                                              cache_size);
  if (NULL == cache) {
    printf("Unable to initialize cache.\n");
    return;
  }
  printf("Iteration %d, initial entries: %d\n", iteration,
         cache->GetEntryCount());

  int seed = static_cast<int>(Time::Now().ToInternalValue());
  srand(seed);

  const int kNumKeys = 5000;
  const int kNumEntries = 30;
  std::string keys[kNumKeys];
  disk_cache::Entry* entries[kNumEntries] = {0};

  for (int i = 0; i < kNumKeys; i++) {
    keys[i] = GenerateKey(true);
  }

  const int kDataLen = 4000;
  char data[kDataLen];
  memset(data, 'k', kDataLen);

  for (int i = 0;; i++) {
    int slot = rand() % kNumEntries;
    int key = rand() % kNumKeys;

    if (entries[slot])
      entries[slot]->Close();

    if (!cache->OpenEntry(keys[key], &entries[slot]))
      CHECK(cache->CreateEntry(keys[key], &entries[slot]));

    base::snprintf(data, kDataLen, "%d %d", iteration, i);
    CHECK(kDataLen == entries[slot]->WriteData(0, 0, data, kDataLen, NULL,
                                               false));

    if (rand() % 100 > 80) {
      key = rand() % kNumKeys;
      cache->DoomEntry(keys[key]);
    }

    if (!(i % 100))
      printf("Entries: %d    \r", i);
  }
}

// We want to prevent the timer thread from killing the process while we are
// waiting for the debugger to attach.
bool g_crashing = false;

class CrashTask : public Task {
 public:
  CrashTask() {}
  ~CrashTask() {}

  virtual void Run() {
    // Keep trying to run.
    RunSoon(MessageLoop::current());

    if (g_crashing)
      return;

    if (rand() % 100 > 1) {
      printf("sweet death...\n");
#if defined(OS_WIN)
      // Windows does more work on _exit() that we would like, so we use Kill.
      base::KillProcess(base::GetCurrentProcId(), kExpectedCrash, false);
#elif defined(OS_POSIX)
      // On POSIX, _exit() will terminate the process with minimal cleanup,
      // and it is cleaner than killing.
      _exit(kExpectedCrash);
#endif
    }
  }

  static void RunSoon(MessageLoop* target_loop) {
    int task_delay = 10000;  // 10 seconds
    CrashTask* task = new CrashTask();
    target_loop->PostDelayedTask(FROM_HERE, task, task_delay);
  }
};

// We leak everything here :)
bool StartCrashThread() {
  base::Thread* thread = new base::Thread("party_crasher");
  if (!thread->Start())
    return false;

  CrashTask::RunSoon(thread->message_loop());
  return true;
}

void CrashHandler(const std::string& str) {
  g_crashing = true;
  DebugUtil::BreakDebugger();
}

// -----------------------------------------------------------------------

int main(int argc, const char* argv[]) {
  // Setup an AtExitManager so Singleton objects will be destructed.
  base::AtExitManager at_exit_manager; 

  if (argc < 2)
    return MasterCode();

  logging::SetLogAssertHandler(CrashHandler);

  // Some time for the memory manager to flush stuff.
  PlatformThread::Sleep(3000);
  MessageLoop message_loop;

  char* end;
  long int iteration = strtol(argv[1], &end, 0);

  if (!StartCrashThread()) {
    printf("failed to start thread\n");
    return kError;
  }

  StressTheCache(iteration);
  return 0;
}

