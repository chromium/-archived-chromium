// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>
#include <tlhelp32.h>     // for CreateToolhelp32Snapshot()
#include <map>

#include "tools/memory_watcher/memory_watcher.h"
#include "base/file_util.h"
#include "base/lock.h"
#include "base/logging.h"
#include "base/stats_counters.h"
#include "base/string_util.h"
#include "tools/memory_watcher/call_stack.h"
#include "tools/memory_watcher/preamble_patcher.h"

static StatsCounter mem_in_use("MemoryInUse.Bytes");
static StatsCounter mem_in_use_blocks("MemoryInUse.Blocks");
static StatsCounter mem_in_use_allocs("MemoryInUse.Allocs");
static StatsCounter mem_in_use_frees("MemoryInUse.Frees");

// ---------------------------------------------------------------------

MemoryWatcher::MemoryWatcher()
  : file_(NULL),
    hooked_(false),
    in_track_(false),
    block_map_size_(0) {
  MemoryHook::Initialize();
  CallStack::Initialize();

  block_map_ = new CallStackMap();
  stack_map_ = new CallStackIdMap();

  // Register last - only after we're ready for notifications!
  Hook();
}

MemoryWatcher::~MemoryWatcher() {
  Unhook();

  CloseLogFile();

  // Pointers in the block_map are part of the MemoryHook heap.  Be sure
  // to delete the map before closing the heap.
  delete block_map_;
}

void MemoryWatcher::Hook() {
  DCHECK(!hooked_);
  MemoryHook::RegisterWatcher(this);
  hooked_ = true;
}

void MemoryWatcher::Unhook() {
  if (hooked_) {
    MemoryHook::UnregisterWatcher(this);
    hooked_ = false;
  }
}

void MemoryWatcher::OpenLogFile() {
  DCHECK(file_ == NULL);
  file_name_ = "memwatcher";
  if (!log_name_.empty()) {
    file_name_ += ".";
    file_name_ += log_name_;
  }
  file_name_ += ".log";
  char buf[16];
  file_name_ += _itoa(GetCurrentProcessId(), buf, 10);

  std::string tmp_name(file_name_);
  tmp_name += ".tmp";
  file_ = fopen(tmp_name.c_str(), "w+");
}

void MemoryWatcher::CloseLogFile() {
  if (file_ != NULL) {
    fclose(file_);
    file_ = NULL;
    std::wstring tmp_name = ASCIIToWide(file_name_);
    tmp_name += L".tmp";
    file_util::Move(tmp_name, ASCIIToWide(file_name_));
  }
}

void MemoryWatcher::OnTrack(HANDLE heap, int32 id, int32 size) {
  // AllocationStack overrides new/delete to not allocate
  // from the main heap.
  AllocationStack* stack = new AllocationStack(size);
  {
    // Don't track zeroes.  It's a waste of time.
    if (size == 0) {
      delete stack;
      return;
    }

    AutoLock lock(block_map_lock_);

    // Ideally, we'd like to verify that the block being added
    // here is not already in our list of tracked blocks.  However,
    // the lookup in our hash table is expensive and slows us too
    // much.  Uncomment this line if you think you need it.
    //DCHECK(block_map_->find(id) == block_map_->end());

    (*block_map_)[id] = stack;

    CallStackIdMap::iterator it = stack_map_->find(stack->hash());
    if (it != stack_map_->end()) {
      it->second.size += size;
      it->second.count++;
    } else {
      StackTrack tracker;
      tracker.count = 1;
      tracker.size = size;
      tracker.stack = stack;
      (*stack_map_)[stack->hash()] = tracker;
    }

    block_map_size_ += size;
  }

  mem_in_use.Set(block_map_size_);
  mem_in_use_blocks.Increment();
  mem_in_use_allocs.Increment();
}

void MemoryWatcher::OnUntrack(HANDLE heap, int32 id, int32 size) {
  DCHECK(size >= 0);

  // Don't bother with these.
  if (size == 0)
    return;

  {
    AutoLock lock(block_map_lock_);

    // First, find the block in our block_map.
    CallStackMap::iterator it = block_map_->find(id);
    if (it != block_map_->end()) {
      AllocationStack* stack = it->second;
      CallStackIdMap::iterator id_it = stack_map_->find(stack->hash());
      DCHECK(id_it != stack_map_->end());
      id_it->second.size -= size;
      id_it->second.count--;
      DCHECK(id_it->second.count >= 0);

      // If there are no more callstacks with this stack, then we
      // have cleaned up all instances, and can safely delete the
      // stack pointer in the stack_map.
      bool safe_to_delete = true;
      if (id_it->second.count == 0)
        stack_map_->erase(id_it);
      else if (id_it->second.stack == stack)
        safe_to_delete = false;  // we're still using the stack

      block_map_size_ -= size;
      block_map_->erase(id);
      if (safe_to_delete)
        delete stack;
    } else {
      // Untracked item.  This happens a fair amount, and it is
      // normal.  A lot of time elapses during process startup
      // before the allocation routines are hooked.
    }
  }

  mem_in_use.Set(block_map_size_);
  mem_in_use_blocks.Decrement();
  mem_in_use_frees.Increment();
}

void MemoryWatcher::SetLogName(char* log_name) {
  if (!log_name)
    return;

  log_name_ = log_name;
}

void MemoryWatcher::DumpLeaks() {
  // We can only dump the leaks once.  We'll cleanup the hooks here.
  DCHECK(hooked_);
  Unhook();

  AutoLock lock(block_map_lock_);

  OpenLogFile();

  // Dump the stack map.
  CallStackIdMap::iterator it = stack_map_->begin();
  while (it != stack_map_->end()) {
    fwprintf(file_, L"%d bytes, %d items (0x%x)\n",
             it->second.size, it->second.count, it->first);
    CallStack* stack = it->second.stack;
    std::string output;
    stack->ToString(&output);
    fprintf(file_, "%s", output.c_str());
    it++;
  }
  fprintf(file_, "Total Leaks:  %d\n", block_map_->size());
  fprintf(file_, "Total Stacks: %d\n", stack_map_->size());
  fprintf(file_, "Total Bytes:  %d\n", block_map_size_);
  CloseLogFile();
}
