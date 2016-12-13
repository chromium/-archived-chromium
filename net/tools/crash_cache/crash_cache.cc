// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This command-line program generates the set of files needed for the crash-
// cache unit tests (DiskCacheTest,CacheBackend_Recover*). This program only
// works properly on debug mode, because the crash functionality is not compiled
// on release builds of the cache.

#include <string>

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/path_service.h"
#include "base/process_util.h"
#include "base/string_util.h"

#include "net/disk_cache/backend_impl.h"
#include "net/disk_cache/disk_cache.h"
#include "net/disk_cache/disk_cache_test_util.h"
#include "net/disk_cache/rankings.h"

using base::Time;

enum Errors {
  GENERIC = -1,
  ALL_GOOD = 0,
  INVALID_ARGUMENT = 1,
  CRASH_OVERWRITE,
  NOT_REACHED
};

using disk_cache::RankCrashes;

// Starts a new process, to generate the files.
int RunSlave(RankCrashes action) {
  FilePath exe;
  PathService::Get(base::FILE_EXE, &exe);

  CommandLine cmdline(exe.ToWStringHack());
  cmdline.AppendLooseValue(ASCIIToWide(IntToString(action)));

  base::ProcessHandle handle;
  if (!base::LaunchApp(cmdline, false, false, &handle)) {
    printf("Unable to run test %d\n", action);
    return GENERIC;
  }

  int exit_code;

  if (!base::WaitForExitCode(handle, &exit_code)) {
    printf("Unable to get return code, test %d\n", action);
    return GENERIC;
  }
  if (ALL_GOOD != exit_code)
    printf("Test %d failed, code %d\n", action, exit_code);

  return exit_code;
}

// Main loop for the master process.
int MasterCode() {
  for (int i = disk_cache::NO_CRASH + 1; i < disk_cache::MAX_CRASH; i++) {
    int ret = RunSlave(static_cast<RankCrashes>(i));
    if (ALL_GOOD != ret)
      return ret;
  }

  return ALL_GOOD;
}

// -----------------------------------------------------------------------

extern RankCrashes g_rankings_crash;
const char* kCrashEntryName = "the first key";

// Creates the destinaton folder for this run, and returns it on full_path.
bool CreateTargetFolder(const std::wstring& path, RankCrashes action,
                        std::wstring* full_path) {
  const wchar_t* folders[] = {
    L"",
    L"insert_empty1",
    L"insert_empty2",
    L"insert_empty3",
    L"insert_one1",
    L"insert_one2",
    L"insert_one3",
    L"insert_load1",
    L"insert_load2",
    L"remove_one1",
    L"remove_one2",
    L"remove_one3",
    L"remove_one4",
    L"remove_head1",
    L"remove_head2",
    L"remove_head3",
    L"remove_head4",
    L"remove_tail1",
    L"remove_tail2",
    L"remove_tail3",
    L"remove_load1",
    L"remove_load2",
    L"remove_load3"
  };
  COMPILE_ASSERT(arraysize(folders) == disk_cache::MAX_CRASH, sync_folders);
  DCHECK(action > disk_cache::NO_CRASH && action < disk_cache::MAX_CRASH);

  *full_path = path;
  file_util::AppendToPath(full_path, folders[action]);

  if (file_util::PathExists(*full_path))
    return false;

  return file_util::CreateDirectory(*full_path);
}

// Generates the files for an empty and one item cache.
int SimpleInsert(const std::wstring& path, RankCrashes action) {
  disk_cache::Backend* cache = disk_cache::CreateCacheBackend(path, false, 0,
                                                              net::DISK_CACHE);
  if (!cache || cache->GetEntryCount())
    return GENERIC;

  const char* test_name = "some other key";

  if (action <= disk_cache::INSERT_EMPTY_3) {
    test_name = kCrashEntryName;
    g_rankings_crash = action;
  }

  disk_cache::Entry* entry;
  if (!cache->CreateEntry(test_name, &entry))
    return GENERIC;

  entry->Close();

  DCHECK(action <= disk_cache::INSERT_ONE_3);
  g_rankings_crash = action;
  test_name = kCrashEntryName;

  if (!cache->CreateEntry(test_name, &entry))
    return GENERIC;

  return NOT_REACHED;
}

// Generates the files for a one item cache, and removing the head.
int SimpleRemove(const std::wstring& path, RankCrashes action) {
  DCHECK(action >= disk_cache::REMOVE_ONE_1);
  DCHECK(action <= disk_cache::REMOVE_TAIL_3);

  disk_cache::Backend* cache = disk_cache::CreateCacheBackend(path, false, 0,
                                                              net::DISK_CACHE);
  if (!cache || cache->GetEntryCount())
    return GENERIC;

  disk_cache::Entry* entry;
  if (!cache->CreateEntry(kCrashEntryName, &entry))
    return GENERIC;

  entry->Close();

  if (action >= disk_cache::REMOVE_TAIL_1) {
    if (!cache->CreateEntry("some other key", &entry))
      return GENERIC;

    entry->Close();
  }

  if (!cache->OpenEntry(kCrashEntryName, &entry))
    return GENERIC;

  g_rankings_crash = action;
  entry->Doom();
  entry->Close();

  return NOT_REACHED;
}

int HeadRemove(const std::wstring& path, RankCrashes action) {
  DCHECK(action >= disk_cache::REMOVE_HEAD_1);
  DCHECK(action <= disk_cache::REMOVE_HEAD_4);

  disk_cache::Backend* cache = disk_cache::CreateCacheBackend(path, false, 0,
                                                              net::DISK_CACHE);
  if (!cache || cache->GetEntryCount())
    return GENERIC;

  disk_cache::Entry* entry;
  if (!cache->CreateEntry("some other key", &entry))
    return GENERIC;

  entry->Close();
  if (!cache->CreateEntry(kCrashEntryName, &entry))
    return GENERIC;

  entry->Close();

  if (!cache->OpenEntry(kCrashEntryName, &entry))
    return GENERIC;

  g_rankings_crash = action;
  entry->Doom();
  entry->Close();

  return NOT_REACHED;
}

// Generates the files for insertion and removals on heavy loaded caches.
int LoadOperations(const std::wstring& path, RankCrashes action) {
  DCHECK(action >= disk_cache::INSERT_LOAD_1);

  // Work with a tiny index table (16 entries)
  disk_cache::BackendImpl* cache = new disk_cache::BackendImpl(path, 0xf);
  if (!cache || !cache->SetMaxSize(0x100000) || !cache->Init() ||
      cache->GetEntryCount())
    return GENERIC;

  int seed = static_cast<int>(Time::Now().ToInternalValue());
  srand(seed);

  disk_cache::Entry* entry;
  for (int i = 0; i < 100; i++) {
    std::string key = GenerateKey(true);
    if (!cache->CreateEntry(key, &entry))
      return GENERIC;
    entry->Close();
    if (50 == i && action >= disk_cache::REMOVE_LOAD_1) {
      if (!cache->CreateEntry(kCrashEntryName, &entry))
        return GENERIC;
      entry->Close();
    }
  }

  if (action <= disk_cache::INSERT_LOAD_2) {
    g_rankings_crash = action;

    if (!cache->CreateEntry(kCrashEntryName, &entry))
      return GENERIC;
  }

  if (!cache->OpenEntry(kCrashEntryName, &entry))
    return GENERIC;

  g_rankings_crash = action;

  entry->Doom();
  entry->Close();

  return NOT_REACHED;
}

// Main function on the child process.
int SlaveCode(const std::wstring& path, RankCrashes action) {
  MessageLoop message_loop;

  std::wstring full_path;
  if (!CreateTargetFolder(path, action, &full_path)) {
    printf("Destination folder found, please remove it.\n");
    return CRASH_OVERWRITE;
  }

  if (action <= disk_cache::INSERT_ONE_3)
    return SimpleInsert(full_path, action);

  if (action <= disk_cache::INSERT_LOAD_2)
    return LoadOperations(full_path, action);

  if (action <= disk_cache::REMOVE_ONE_4)
    return SimpleRemove(full_path, action);

  if (action <= disk_cache::REMOVE_HEAD_4)
    return HeadRemove(full_path, action);

  if (action <= disk_cache::REMOVE_TAIL_3)
    return SimpleRemove(full_path, action);

  if (action <= disk_cache::REMOVE_LOAD_3)
    return LoadOperations(full_path, action);

  return NOT_REACHED;
}

// -----------------------------------------------------------------------

int main(int argc, const char* argv[]) {
  // Setup an AtExitManager so Singleton objects will be destructed.
  base::AtExitManager at_exit_manager;

  if (argc < 2)
    return MasterCode();

  char* end;
  RankCrashes action = static_cast<RankCrashes>(strtol(argv[1], &end, 0));
  if (action <= disk_cache::NO_CRASH || action >= disk_cache::MAX_CRASH) {
    printf("Invalid action\n");
    return INVALID_ARGUMENT;
  }

  FilePath path;
  PathService::Get(base::DIR_SOURCE_ROOT, &path);
  path = path.AppendASCII("net");
  path = path.AppendASCII("data");
  path = path.AppendASCII("cache_tests");
  path = path.AppendASCII("new_crashes");

  return SlaveCode(path.ToWStringHack(), action);
}
