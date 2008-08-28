// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is a simple application that stress-tests the crash recovery of the disk
// cache. The main application starts a copy of itself on a loop, checking the
// exit code of the child process. When the child dies in an unexpected way,
// the main application quits.

// The child application has two threads: one to exercise the cache in an
// infinite loop, and another one to asynchronously kill the process.

#include <windows.h>
#include <string>

#include "base/at_exit.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/path_service.h"
#include "base/string_util.h"
#include "base/thread.h"
#include "net/disk_cache/disk_cache.h"
#include "net/disk_cache/disk_cache_test_util.h"

const int kError = -1;
const int kExpectedCrash = 1000000;

// Starts a new process.
int RunSlave(int iteration) {
  std::wstring exe;
  PathService::Get(base::FILE_EXE, &exe);

  std::wstring command = StringPrintf(L"%ls %d", exe.c_str(), iteration);

  STARTUPINFO startup_info = {0};
  startup_info.cb = sizeof(startup_info);
  PROCESS_INFORMATION process_info;

  // I really don't care about this call modifying the string.
  if (!::CreateProcess(exe.c_str(), const_cast<wchar_t*>(command.c_str()), NULL,
                       NULL, FALSE, 0, NULL, NULL, &startup_info,
                       &process_info)) {
    printf("Unable to run test\n");
    return kError;
  }

  DWORD reason = ::WaitForSingleObject(process_info.hProcess, INFINITE);

  int code;
  bool ok = ::GetExitCodeProcess(process_info.hProcess,
                                 reinterpret_cast<PDWORD>(&code)) ? true :
                                                                    false;

  ::CloseHandle(process_info.hProcess);
  ::CloseHandle(process_info.hThread);

  if (!ok) {
    printf("Unable to get return code\n");
    return kError;
  }

  return code;
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

    sprintf_s(data, "%d %d", iteration, i);
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
      TerminateProcess(GetCurrentProcess(), kExpectedCrash);
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
  __debugbreak();
}

// -----------------------------------------------------------------------

int main(int argc, const char* argv[]) {
  // Setup an AtExitManager so Singleton objects will be destructed.
  base::AtExitManager at_exit_manager; 

  if (argc < 2)
    return MasterCode();

  logging::SetLogAssertHandler(CrashHandler);

  // Some time for the memory manager to flush stuff.
  Sleep(3000);
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

