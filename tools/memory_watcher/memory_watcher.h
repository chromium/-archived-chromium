// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// MemoryWatcher.
// The MemoryWatcher is a library that can be linked into any
// win32 application.  It will override the default memory allocators
// and track call stacks for any allocations that are made.  It can
// then be used to see what memory is in use.

#ifndef MEMORY_WATCHER_MEMORY_WATCHER_
#define MEMORY_WATCHER_MEMORY_WATCHER_

#include <map>
#include <functional>
#include "base/lock.h"
#include "base/logging.h"
#include "tools/memory_watcher/memory_hook.h"

class CallStack;
class AllocationStack;

// The MemoryWatcher installs allocation hooks and monitors
// allocations and frees.
class MemoryWatcher : MemoryObserver {
 public:
  MemoryWatcher();
  virtual ~MemoryWatcher();

  // Dump all tracked pointers still in use.
  void DumpLeaks();

  // MemoryObserver interface.
  virtual void OnTrack(HANDLE heap, int32 id, int32 size);
  virtual void OnUntrack(HANDLE heap, int32 id, int32 size);

  // Sets a name that appears in the generated file name.
  void SetLogName(char* log_name);

 private:
  // Opens the logfile which we create.
  void OpenLogFile();

  // Close the logfile.
  void CloseLogFile();

  // Hook the memory hooks.
  void Hook();

  // Unhooks our memory hooks.
  void Unhook();

  // This is for logging.
  FILE* file_;

  struct StackTrack {
    CallStack* stack;
    int count;
    int size;
  };

  bool hooked_;  // True when this class has the memory_hooks hooked.

  bool in_track_;
  Lock block_map_lock_;
  typedef std::map<int32, AllocationStack*, std::less<int32>,
                   PrivateHookAllocator<int32>> CallStackMap;
  typedef std::map<int32, StackTrack, std::less<int32>,
                   PrivateHookAllocator<int32>> CallStackIdMap;
  // The block_map provides quick lookups based on the allocation
  // pointer.  This is important for having fast round trips through
  // malloc/free.
  CallStackMap *block_map_;
  // The stack_map keeps track of the known CallStacks based on the
  // hash of the CallStack.  This is so that we can quickly aggregate
  // like-CallStacks together.
  CallStackIdMap *stack_map_;
  int32 block_map_size_;

  // The file name for that log.
  std::string file_name_;

  // An optional name that appears in the log file name (used to differentiate
  // logs).
  std::string log_name_;
};



#endif  // MEMORY_WATCHER_
