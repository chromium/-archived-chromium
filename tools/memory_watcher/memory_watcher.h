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
